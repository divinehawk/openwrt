// SPDX-License-Identifier: GPL-2.0-only
/*
 * Native ucode binding for the Linux net/xpon Generic Netlink API.
 *
 * xPON control is intentionally kept in-process: rpcd imports this module
 * directly and no shell helper is inserted between ubus and the kernel.
 */

#include <errno.h>
#include <linux/genetlink.h>
#include <linux/netlink.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <ucode/module.h>

#include "include/linux/xpon.h"

#define XPON_NL_REQ_SIZE 4096
#define XPON_NL_RX_SIZE  16384
#define XPON_NL_TIMEOUT_SEC 2

#ifndef NLA_TYPE_MASK
#define NLA_TYPE_MASK ~(NLA_F_NESTED | NLA_F_NET_BYTEORDER)
#endif

struct xpon_nl {
	int fd;
	uint16_t family_id;
	uint32_t family_version;
	uint32_t portid;
	uint32_t seq;
};

struct nl_request {
	unsigned char buf[XPON_NL_REQ_SIZE];
	struct nlmsghdr *nlh;
	struct genlmsghdr *genl;
};

static struct {
	int code;
	char message[192];
} last_error;

static void set_error(int code, const char *message)
{
	last_error.code = code;
	if (!message)
		message = code ? strerror(code < 0 ? -code : code) : "success";
	snprintf(last_error.message, sizeof(last_error.message), "%s", message);
}

static void set_errno_error(int code, const char *where)
{
	char buf[sizeof(last_error.message)];
	int e = code < 0 ? -code : code;

	snprintf(buf, sizeof(buf), "%s: %s", where, strerror(e));
	set_error(-e, buf);
}

static inline size_t nla_align_len(size_t len)
{
	return (len + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1);
}

static inline void *nla_data_ptr(const struct nlattr *attr)
{
	return (char *)attr + NLA_HDRLEN;
}

static inline int nla_payload_len(const struct nlattr *attr)
{
	return (int)attr->nla_len - NLA_HDRLEN;
}

static bool attr_ok(const struct nlattr *attr, int remaining)
{
	return remaining >= (int)sizeof(*attr) &&
	       attr->nla_len >= sizeof(*attr) && attr->nla_len <= remaining;
}

static struct nlattr *attr_next(struct nlattr *attr, int *remaining)
{
	int step = (int)nla_align_len(attr->nla_len);

	*remaining -= step;
	return (struct nlattr *)((char *)attr + step);
}

static void parse_attrs(struct nlattr **tb, unsigned int max,
			struct nlattr *attr, int remaining)
{
	memset(tb, 0, sizeof(*tb) * (max + 1));
	while (attr_ok(attr, remaining)) {
		unsigned int type = attr->nla_type & NLA_TYPE_MASK;

		if (type <= max)
			tb[type] = attr;
		attr = attr_next(attr, &remaining);
	}
}

static uint8_t attr_u8(const struct nlattr *attr)
{
	uint8_t value = 0;

	if (attr && nla_payload_len(attr) >= (int)sizeof(value))
		memcpy(&value, nla_data_ptr(attr), sizeof(value));
	return value;
}

static uint16_t attr_u16(const struct nlattr *attr)
{
	uint16_t value = 0;

	if (attr && nla_payload_len(attr) >= (int)sizeof(value))
		memcpy(&value, nla_data_ptr(attr), sizeof(value));
	return value;
}

static uint32_t attr_u32(const struct nlattr *attr)
{
	uint32_t value = 0;

	if (attr && nla_payload_len(attr) >= (int)sizeof(value))
		memcpy(&value, nla_data_ptr(attr), sizeof(value));
	return value;
}

static int32_t attr_s32(const struct nlattr *attr)
{
	int32_t value = 0;

	if (attr && nla_payload_len(attr) >= (int)sizeof(value))
		memcpy(&value, nla_data_ptr(attr), sizeof(value));
	return value;
}

static uint64_t attr_u64(const struct nlattr *attr)
{
	uint64_t value = 0;

	if (attr && nla_payload_len(attr) >= (int)sizeof(value))
		memcpy(&value, nla_data_ptr(attr), sizeof(value));
	return value;
}

static const char *attr_string(const struct nlattr *attr)
{
	const char *value;
	int len;

	if (!attr)
		return NULL;
	len = nla_payload_len(attr);
	if (len <= 0)
		return NULL;
	value = nla_data_ptr(attr);
	return value[len - 1] == '\0' ? value : NULL;
}

static void request_init(struct xpon_nl *ctx, struct nl_request *req,
			 uint16_t type, uint8_t cmd, uint8_t version,
			 uint16_t flags)
{
	memset(req, 0, sizeof(*req));
	req->nlh = (struct nlmsghdr *)req->buf;
	req->nlh->nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
	req->nlh->nlmsg_type = type;
	req->nlh->nlmsg_flags = NLM_F_REQUEST | flags;
	req->nlh->nlmsg_seq = ++ctx->seq;
	req->nlh->nlmsg_pid = ctx->portid;
	req->genl = (struct genlmsghdr *)NLMSG_DATA(req->nlh);
	req->genl->cmd = cmd;
	req->genl->version = version;
}

static int add_attr(struct nl_request *req, uint16_t type,
		    const void *data, size_t len)
{
	size_t attr_len = NLA_HDRLEN + len;
	size_t aligned = nla_align_len(attr_len);
	struct nlattr *attr;

	if (NLMSG_ALIGN(req->nlh->nlmsg_len) + aligned > sizeof(req->buf))
		return -EMSGSIZE;

	attr = (struct nlattr *)(req->buf + NLMSG_ALIGN(req->nlh->nlmsg_len));
	attr->nla_type = type;
	attr->nla_len = attr_len;
	if (len)
		memcpy(nla_data_ptr(attr), data, len);
	if (aligned > attr_len)
		memset((char *)attr + attr_len, 0, aligned - attr_len);
	req->nlh->nlmsg_len = NLMSG_ALIGN(req->nlh->nlmsg_len) + aligned;
	return 0;
}

static int add_u8(struct nl_request *req, uint16_t type, uint8_t value)
{
	return add_attr(req, type, &value, sizeof(value));
}

static int add_u32(struct nl_request *req, uint16_t type, uint32_t value)
{
	return add_attr(req, type, &value, sizeof(value));
}

static int add_nested_u8(struct nl_request *req, uint16_t type,
			 uint16_t nested_type, uint8_t value)
{
	unsigned char payload[NLA_ALIGN(NLA_HDRLEN + sizeof(value))] = {};
	struct nlattr *nested = (struct nlattr *)payload;

	nested->nla_type = nested_type;
	nested->nla_len = NLA_HDRLEN + sizeof(value);
	memcpy(nla_data_ptr(nested), &value, sizeof(value));

	return add_attr(req, type | NLA_F_NESTED, payload, sizeof(payload));
}

static int nl_send_request(struct xpon_nl *ctx, struct nl_request *req)
{
	struct sockaddr_nl peer = { .nl_family = AF_NETLINK };
	ssize_t n;

	n = sendto(ctx->fd, req->buf, req->nlh->nlmsg_len, 0,
		   (struct sockaddr *)&peer, sizeof(peer));
	if (n < 0)
		return -errno;
	if ((size_t)n != req->nlh->nlmsg_len)
		return -EIO;
	return 0;
}

typedef int (*reply_cb_t)(struct nlmsghdr *nlh, void *arg);

static int recv_for_seq(struct xpon_nl *ctx, uint32_t seq,
			reply_cb_t cb, void *arg)
{
	unsigned char buf[XPON_NL_RX_SIZE];
	int ret;

	for (;;) {
		ssize_t len = recv(ctx->fd, buf, sizeof(buf), 0);
		struct nlmsghdr *nlh;
		int remaining;

		if (len < 0)
			return -errno;
		if (!len)
			return -ECONNRESET;

		remaining = len;
		for (nlh = (struct nlmsghdr *)buf; NLMSG_OK(nlh, remaining);
		     nlh = NLMSG_NEXT(nlh, remaining)) {
			if (nlh->nlmsg_seq != seq)
				continue;
			if (nlh->nlmsg_type == NLMSG_ERROR) {
				struct nlmsgerr *e;

				if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(*e)))
					return -EBADMSG;
				e = NLMSG_DATA(nlh);
				return e->error;
			}
			if (nlh->nlmsg_type == NLMSG_DONE)
				return 0;
			if (!cb)
				return 0;
			ret = cb(nlh, arg);
			return ret;
		}
	}
}

static int request_exec(struct xpon_nl *ctx, struct nl_request *req,
			bool expect_reply, reply_cb_t cb, void *arg)
{
	uint32_t seq = req->nlh->nlmsg_seq;
	int ret;

	if (!expect_reply)
		req->nlh->nlmsg_flags |= NLM_F_ACK;
	ret = nl_send_request(ctx, req);
	if (ret)
		return ret;
	return recv_for_seq(ctx, seq, expect_reply ? cb : NULL, arg);
}

static int resolve_family_reply(struct nlmsghdr *nlh, void *arg)
{
	struct xpon_nl *ctx = arg;
	struct genlmsghdr *genl;
	struct nlattr *attr;
	int remaining;

	if (nlh->nlmsg_len < NLMSG_LENGTH(GENL_HDRLEN))
		return -EBADMSG;
	genl = NLMSG_DATA(nlh);
	remaining = nlh->nlmsg_len - NLMSG_HDRLEN - GENL_HDRLEN;
	attr = (struct nlattr *)((char *)genl + GENL_HDRLEN);
	while (attr_ok(attr, remaining)) {
		switch (attr->nla_type & NLA_TYPE_MASK) {
		case CTRL_ATTR_FAMILY_ID:
			ctx->family_id = attr_u16(attr);
			break;
		case CTRL_ATTR_VERSION:
			ctx->family_version = attr_u32(attr);
			break;
		default:
			break;
		}
		attr = attr_next(attr, &remaining);
	}
	return ctx->family_id ? 0 : -ENOENT;
}

static int resolve_family(struct xpon_nl *ctx)
{
	struct nl_request req;
	int ret;

	request_init(ctx, &req, GENL_ID_CTRL, CTRL_CMD_GETFAMILY, 1, 0);
	ret = add_attr(&req, CTRL_ATTR_FAMILY_NAME, XPON_GENL_NAME,
		       strlen(XPON_GENL_NAME) + 1);
	if (ret)
		return ret;
	return request_exec(ctx, &req, true, resolve_family_reply, ctx);
}

static int xpon_nl_open(struct xpon_nl *ctx)
{
	struct sockaddr_nl local = { .nl_family = AF_NETLINK };
	struct timeval tv = { .tv_sec = XPON_NL_TIMEOUT_SEC };
	socklen_t addrlen = sizeof(local);
	int ret;

	memset(ctx, 0, sizeof(*ctx));
	ctx->fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_GENERIC);
	if (ctx->fd < 0)
		return -errno;
	setsockopt(ctx->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(ctx->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (bind(ctx->fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
		ret = -errno;
		goto fail;
	}
	if (getsockname(ctx->fd, (struct sockaddr *)&local, &addrlen) < 0) {
		ret = -errno;
		goto fail;
	}
	ctx->portid = local.nl_pid;
	ctx->seq = (uint32_t)getpid();
	ret = resolve_family(ctx);
	if (ret)
		goto fail;
	return 0;

fail:
	close(ctx->fd);
	ctx->fd = -1;
	return ret;
}

static void xpon_nl_close(struct xpon_nl *ctx)
{
	if (ctx->fd >= 0)
		close(ctx->fd);
	ctx->fd = -1;
}

static int get_reply_attrs(struct nlmsghdr *nlh, struct nlattr **tb,
			   unsigned int max)
{
	struct genlmsghdr *genl;
	int remaining;

	if (nlh->nlmsg_len < NLMSG_LENGTH(GENL_HDRLEN))
		return -EBADMSG;
	genl = NLMSG_DATA(nlh);
	remaining = nlh->nlmsg_len - NLMSG_HDRLEN - GENL_HDRLEN;
	parse_attrs(tb, max,
		    (struct nlattr *)((char *)genl + GENL_HDRLEN), remaining);
	return 0;
}

static const char *mode_name(uint8_t mode)
{
	switch (mode) {
	case XPON_MODE_GPON:
		return "gpon";
	case XPON_MODE_EPON:
		return "epon";
	case XPON_MODE_XGSPON:
		return "xgspon";
	default:
		return "unknown";
	}
}

static const char *registration_name(uint8_t state)
{
	switch (state) {
	case XPON_REGISTRATION_DOWN:
		return "down";
	case XPON_REGISTRATION_DISCOVERY:
		return "discovery";
	case XPON_REGISTRATION_REGISTERING:
		return "registering";
	case XPON_REGISTRATION_OPERATIONAL:
		return "operational";
	default:
		return "unknown";
	}
}

static bool parse_u32_arg(uc_value_t *v, uint32_t *out)
{
	uint64_t n;

	if (!v)
		return false;
	errno = 0;
	n = ucv_to_unsigned(v);
	if (errno || n > UINT32_MAX)
		return false;
	*out = n;
	return true;
}

static bool parse_bool_arg(uc_value_t *v, uint8_t *out)
{
	uint32_t n;

	if (!parse_u32_arg(v, &n) || n > 1)
		return false;
	*out = (uint8_t)n;
	return true;
}

static int parse_ifindex(uc_value_t *value, uint32_t *ifindex, char *ifname,
			 size_t ifname_len)
{
	uint32_t index;
	const char *name;

	if (!value)
		return -EINVAL;
	if (ucv_type(value) == UC_STRING) {
		name = ucv_string_get(value);
		if (!name || !*name)
			return -EINVAL;
		index = if_nametoindex(name);
		if (!index)
			return errno ? -errno : -ENODEV;
		if (ifname && ifname_len)
			snprintf(ifname, ifname_len, "%s", name);
	}
	else {
		if (!parse_u32_arg(value, &index) || !index)
			return -EINVAL;
		if (ifname && ifname_len && !if_indextoname(index, ifname))
			return errno ? -errno : -ENODEV;
	}
	*ifindex = index;
	return 0;
}

static int parse_mode(uc_value_t *value, uint8_t *mode)
{
	uint32_t n;
	const char *name;

	if (!value)
		return -EINVAL;
	if (ucv_type(value) == UC_STRING) {
		name = ucv_string_get(value);
		if (!strcmp(name, "gpon"))
			*mode = XPON_MODE_GPON;
		else if (!strcmp(name, "epon"))
			*mode = XPON_MODE_EPON;
		else if (!strcmp(name, "xgspon"))
			*mode = XPON_MODE_XGSPON;
		else
			return -EINVAL;
		return 0;
	}
	if (!parse_u32_arg(value, &n) || n > XPON_MODE_MAX)
		return -EINVAL;
	*mode = (uint8_t)n;
	return 0;
}

static const char *optical_protocol_name(uint8_t protocol)
{
	switch (protocol) {
	case XPON_OPTICAL_PROTO_ETHERNET:
		return "ethernet";
	case XPON_OPTICAL_PROTO_EPON:
		return "epon";
	case XPON_OPTICAL_PROTO_GPON:
		return "gpon";
	case XPON_OPTICAL_PROTO_XGPON:
		return "xgpon";
	case XPON_OPTICAL_PROTO_XGSPON:
		return "xgspon";
	case XPON_OPTICAL_PROTO_NGPON2:
		return "ngpon2";
	case XPON_OPTICAL_PROTO_UNSPEC:
	default:
		return "unspecified";
	}
}

static void object_add_attr_string(uc_value_t *object, const char *name,
			   const struct nlattr *attr)
{
	const char *value = attr_string(attr);

	if (value)
		ucv_object_add(object, name, ucv_string_new(value));
}

static int add_optical_status(uc_vm_t *vm, uc_value_t *status,
			      const struct nlattr *nested)
{
	struct nlattr *a[XPON_OPTICAL_ATTR_MAX + 1];
	uc_value_t *optical, *protocols;
	uint32_t protocol_mask = 0;
	const char *model;
	unsigned int i;
	char oui[9];

	if (!nested)
		return 0;
	parse_attrs(a, XPON_OPTICAL_ATTR_MAX, nla_data_ptr(nested),
		    nla_payload_len(nested));

	optical = ucv_object_new(vm);
	if (!optical)
		return -ENOMEM;

	object_add_attr_string(optical, "name", a[XPON_OPTICAL_ATTR_NAME]);
	object_add_attr_string(optical, "type", a[XPON_OPTICAL_ATTR_TYPE]);
	object_add_attr_string(optical, "vendor", a[XPON_OPTICAL_ATTR_VENDOR]);
	object_add_attr_string(optical, "part_number",
			       a[XPON_OPTICAL_ATTR_PART_NUMBER]);
	object_add_attr_string(optical, "serial", a[XPON_OPTICAL_ATTR_SERIAL]);
	object_add_attr_string(optical, "date_code", a[XPON_OPTICAL_ATTR_DATE_CODE]);

	model = attr_string(a[XPON_OPTICAL_ATTR_PART_NUMBER]);
	if (!model)
		model = attr_string(a[XPON_OPTICAL_ATTR_NAME]);
	if (model)
		ucv_object_add(optical, "model", ucv_string_new(model));

	if (a[XPON_OPTICAL_ATTR_VENDOR_OUI] &&
	    nla_payload_len(a[XPON_OPTICAL_ATTR_VENDOR_OUI]) == 3) {
		const uint8_t *v = nla_data_ptr(a[XPON_OPTICAL_ATTR_VENDOR_OUI]);

		snprintf(oui, sizeof(oui), "%02x:%02x:%02x", v[0], v[1], v[2]);
		ucv_object_add(optical, "vendor_oui", ucv_string_new(oui));
	}

	if (a[XPON_OPTICAL_ATTR_CAPABILITIES])
		ucv_object_add(optical, "capabilities",
			       ucv_uint64_new(attr_u32(a[XPON_OPTICAL_ATTR_CAPABILITIES])));
	if (a[XPON_OPTICAL_ATTR_PROTOCOLS]) {
		protocol_mask = attr_u32(a[XPON_OPTICAL_ATTR_PROTOCOLS]);
		ucv_object_add(optical, "supported_protocols",
			       ucv_uint64_new(protocol_mask));
		protocols = ucv_array_new(vm);
		for (i = XPON_OPTICAL_PROTO_ETHERNET;
		     i <= XPON_OPTICAL_PROTO_NGPON2; i++)
			if (protocol_mask & (1U << i))
				ucv_array_push(protocols,
					       ucv_string_new(optical_protocol_name(i)));
		ucv_object_add(optical, "supported_protocol_names", protocols);
	}
	if (a[XPON_OPTICAL_ATTR_PROTOCOL]) {
		uint8_t protocol = attr_u8(a[XPON_OPTICAL_ATTR_PROTOCOL]);

		ucv_object_add(optical, "protocol", ucv_uint64_new(protocol));
		ucv_object_add(optical, "protocol_name",
			       ucv_string_new(optical_protocol_name(protocol)));
	}
	if (a[XPON_OPTICAL_ATTR_TX_RATE])
		ucv_object_add(optical, "tx_rate",
			       ucv_uint64_new(attr_u64(a[XPON_OPTICAL_ATTR_TX_RATE])));
	if (a[XPON_OPTICAL_ATTR_RX_RATE])
		ucv_object_add(optical, "rx_rate",
			       ucv_uint64_new(attr_u64(a[XPON_OPTICAL_ATTR_RX_RATE])));
	if (a[XPON_OPTICAL_ATTR_MODE_FLAGS])
		ucv_object_add(optical, "mode_flags",
			       ucv_uint64_new(attr_u32(a[XPON_OPTICAL_ATTR_MODE_FLAGS])));

#define ADD_BOOL(_attr, _name) \
	do { \
		if (a[_attr]) \
			ucv_object_add(optical, _name, \
				       ucv_boolean_new(!!attr_u8(a[_attr]))); \
	} while (0)
	ADD_BOOL(XPON_OPTICAL_ATTR_PRESENT, "present");
	ADD_BOOL(XPON_OPTICAL_ATTR_READY, "ready");
	ADD_BOOL(XPON_OPTICAL_ATTR_RX_LOS, "rx_los");
	ADD_BOOL(XPON_OPTICAL_ATTR_TX_FAULT, "tx_fault");
	ADD_BOOL(XPON_OPTICAL_ATTR_TX_ENABLED, "tx_enabled");
#undef ADD_BOOL

	if (a[XPON_OPTICAL_ATTR_TEMPERATURE_MC])
		ucv_object_add(optical, "temperature_mc",
			       ucv_int64_new(attr_s32(a[XPON_OPTICAL_ATTR_TEMPERATURE_MC])));
	if (a[XPON_OPTICAL_ATTR_VOLTAGE_UV])
		ucv_object_add(optical, "voltage_uv",
			       ucv_uint64_new(attr_u32(a[XPON_OPTICAL_ATTR_VOLTAGE_UV])));
	if (a[XPON_OPTICAL_ATTR_BIAS_UA])
		ucv_object_add(optical, "bias_ua",
			       ucv_uint64_new(attr_u32(a[XPON_OPTICAL_ATTR_BIAS_UA])));
	if (a[XPON_OPTICAL_ATTR_TX_POWER_NW])
		ucv_object_add(optical, "tx_power_nw",
			       ucv_uint64_new(attr_u32(a[XPON_OPTICAL_ATTR_TX_POWER_NW])));
	if (a[XPON_OPTICAL_ATTR_RX_POWER_NW])
		ucv_object_add(optical, "rx_power_nw",
			       ucv_uint64_new(attr_u32(a[XPON_OPTICAL_ATTR_RX_POWER_NW])));
	if (a[XPON_OPTICAL_ATTR_ALARMS])
		ucv_object_add(optical, "alarms",
			       ucv_uint64_new(attr_u32(a[XPON_OPTICAL_ATTR_ALARMS])));

	ucv_object_add(status, "optical", optical);
	return 0;
}

static int add_oam_status(uc_vm_t *vm, uc_value_t *status,
			  const struct nlattr *nested)
{
	struct nlattr *a[XPON_OAM_ATTR_MAX + 1];
	uc_value_t *oam;

	if (!nested)
		return 0;
	parse_attrs(a, XPON_OAM_ATTR_MAX, nla_data_ptr(nested),
		    nla_payload_len(nested));
	oam = ucv_object_new(vm);
	if (!oam)
		return -ENOMEM;
	if (a[XPON_OAM_ATTR_PRESENT]) {
		bool present = !!attr_u8(a[XPON_OAM_ATTR_PRESENT]);

		ucv_object_add(oam, "present", ucv_boolean_new(present));
		ucv_object_add(status, "oam_present", ucv_boolean_new(present));
	}
	if (a[XPON_OAM_ATTR_ENABLED]) {
		bool enabled = !!attr_u8(a[XPON_OAM_ATTR_ENABLED]);

		ucv_object_add(oam, "enabled", ucv_boolean_new(enabled));
		ucv_object_add(status, "oam_enabled", ucv_boolean_new(enabled));
	}
	ucv_object_add(status, "oam", oam);
	return 0;
}

struct status_reply_arg {
	uc_vm_t *vm;
	struct xpon_nl *ctx;
	const char *ifname;
	uc_value_t *value;
};

static int status_reply(struct nlmsghdr *nlh, void *arg)
{
	struct status_reply_arg *r = arg;
	struct nlattr *a[XPON_ATTR_MAX + 1];
	uc_value_t *o, *modes;
	uint32_t available;
	uint8_t mode, registration;
	unsigned int i;
	int ret;

	ret = get_reply_attrs(nlh, a, XPON_ATTR_MAX);
	if (ret)
		return ret;
	if (!a[XPON_ATTR_IFINDEX] || !a[XPON_ATTR_MODE] ||
	    !a[XPON_ATTR_AVAILABLE_MODES] || !a[XPON_ATTR_REGISTRATION])
		return -EBADMSG;

	o = ucv_object_new(r->vm);
	if (!o)
		return -ENOMEM;
	mode = attr_u8(a[XPON_ATTR_MODE]);
	registration = attr_u8(a[XPON_ATTR_REGISTRATION]);
	available = attr_u32(a[XPON_ATTR_AVAILABLE_MODES]);

	ucv_object_add(o, "ifindex", ucv_uint64_new(attr_u32(a[XPON_ATTR_IFINDEX])));
	ucv_object_add(o, "interface", ucv_string_new(r->ifname));
	ucv_object_add(o, "mode", ucv_uint64_new(mode));
	ucv_object_add(o, "mode_name", ucv_string_new(mode_name(mode)));
	ucv_object_add(o, "available_modes", ucv_uint64_new(available));
	modes = ucv_array_new(r->vm);
	for (i = 0; i <= XPON_MODE_MAX; i++)
		if (available & (1U << i))
			ucv_array_push(modes, ucv_string_new(mode_name(i)));
	ucv_object_add(o, "available_mode_names", modes);
	ucv_object_add(o, "registration", ucv_uint64_new(registration));
	ucv_object_add(o, "registration_name",
		       ucv_string_new(registration_name(registration)));
	if (a[XPON_ATTR_CARRIER])
		ucv_object_add(o, "carrier",
			       ucv_boolean_new(!!attr_u8(a[XPON_ATTR_CARRIER])));
	if (a[XPON_ATTR_SIGNAL_DETECT])
		ucv_object_add(o, "signal_detect",
			       ucv_boolean_new(!!attr_u8(a[XPON_ATTR_SIGNAL_DETECT])));
	if (a[XPON_ATTR_LOS])
		ucv_object_add(o, "los",
			       ucv_boolean_new(!!attr_u8(a[XPON_ATTR_LOS])));

	ret = add_optical_status(r->vm, o, a[XPON_ATTR_OPTICAL]);
	if (ret) {
		ucv_put(o);
		return ret;
	}
	ret = add_oam_status(r->vm, o, a[XPON_ATTR_OAM]);
	if (ret) {
		ucv_put(o);
		return ret;
	}

	ucv_object_add(o, "uapi_version", ucv_uint64_new(r->ctx->family_version));
	ucv_object_add(o, "client_uapi_version", ucv_uint64_new(XPON_GENL_VERSION));
	ucv_object_add(o, "uapi_compatible",
		       ucv_boolean_new(r->ctx->family_version == XPON_GENL_VERSION));
	r->value = o;
	return 0;
}

static uc_value_t *uc_error(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *o = ucv_object_new(vm);

	(void)nargs;
	ucv_object_add(o, "code", ucv_int64_new(last_error.code));
	ucv_object_add(o, "errno",
		       ucv_uint64_new(last_error.code < 0 ? -last_error.code : last_error.code));
	ucv_object_add(o, "message",
		       ucv_string_new(last_error.message[0] ? last_error.message : "no error"));
	return o;
}

static uc_value_t *uc_family(uc_vm_t *vm, size_t nargs)
{
	struct xpon_nl ctx;
	uc_value_t *o;
	int ret;

	(void)nargs;
	ret = xpon_nl_open(&ctx);
	if (ret) {
		set_errno_error(ret, "resolve xPON Generic Netlink family");
		return NULL;
	}
	o = ucv_object_new(vm);
	ucv_object_add(o, "name", ucv_string_new(XPON_GENL_NAME));
	ucv_object_add(o, "id", ucv_uint64_new(ctx.family_id));
	ucv_object_add(o, "version", ucv_uint64_new(ctx.family_version));
	ucv_object_add(o, "client_version", ucv_uint64_new(XPON_GENL_VERSION));
	ucv_object_add(o, "compatible",
		       ucv_boolean_new(ctx.family_version == XPON_GENL_VERSION));
	xpon_nl_close(&ctx);
	set_error(0, "success");
	return o;
}

static uc_value_t *uc_ifindex(uc_vm_t *vm, size_t nargs)
{
	char ifname[IF_NAMESIZE];
	uint32_t ifindex;
	int ret;

	(void)vm;
	(void)nargs;
	ret = parse_ifindex(uc_fn_arg(0), &ifindex, ifname, sizeof(ifname));
	if (ret) {
		set_errno_error(ret, "resolve xPON interface");
		return NULL;
	}
	set_error(0, "success");
	return ucv_uint64_new(ifindex);
}

static uc_value_t *uc_status(uc_vm_t *vm, size_t nargs)
{
	struct status_reply_arg r = { .vm = vm };
	struct xpon_nl ctx;
	struct nl_request req;
	char ifname[IF_NAMESIZE];
	uint32_t ifindex;
	int ret;

	(void)nargs;
	ret = parse_ifindex(uc_fn_arg(0), &ifindex, ifname, sizeof(ifname));
	if (ret) {
		set_errno_error(ret, "resolve xPON interface");
		return NULL;
	}
	ret = xpon_nl_open(&ctx);
	if (ret)
		goto fail_open;
	r.ctx = &ctx;
	r.ifname = ifname;
	request_init(&ctx, &req, ctx.family_id, XPON_CMD_GET_STATE,
		     XPON_GENL_VERSION, 0);
	ret = add_u32(&req, XPON_ATTR_IFINDEX, ifindex);
	if (!ret)
		ret = request_exec(&ctx, &req, true, status_reply, &r);
	xpon_nl_close(&ctx);
	if (ret) {
		set_errno_error(ret, "xPON status request");
		return NULL;
	}
	set_error(0, "success");
	return r.value;

fail_open:
	set_errno_error(ret, "open xPON netlink");
	return NULL;
}

static uc_value_t *uc_set_mode(uc_vm_t *vm, size_t nargs)
{
	struct xpon_nl ctx;
	struct nl_request req;
	char ifname[IF_NAMESIZE];
	uint32_t ifindex;
	uint8_t mode;
	int ret;

	(void)vm;
	(void)nargs;
	ret = parse_ifindex(uc_fn_arg(0), &ifindex, ifname, sizeof(ifname));
	if (ret) {
		set_errno_error(ret, "resolve xPON interface");
		return NULL;
	}
	ret = parse_mode(uc_fn_arg(1), &mode);
	if (ret) {
		set_error(ret, "set_mode(): expected gpon, epon, xgspon or a valid mode id");
		return NULL;
	}
	ret = xpon_nl_open(&ctx);
	if (ret)
		goto fail_open;
	request_init(&ctx, &req, ctx.family_id, XPON_CMD_SET_MODE,
		     XPON_GENL_VERSION, 0);
	ret = add_u32(&req, XPON_ATTR_IFINDEX, ifindex);
	if (!ret)
		ret = add_u8(&req, XPON_ATTR_MODE, mode);
	if (!ret)
		ret = request_exec(&ctx, &req, false, NULL, NULL);
	xpon_nl_close(&ctx);
	if (ret) {
		set_errno_error(ret, "xPON mode change");
		return NULL;
	}
	set_error(0, "success");
	return ucv_boolean_new(true);

fail_open:
	set_errno_error(ret, "open xPON netlink");
	return NULL;
}

static uc_value_t *uc_set_oam_enabled(uc_vm_t *vm, size_t nargs)
{
	struct xpon_nl ctx;
	struct nl_request req;
	char ifname[IF_NAMESIZE];
	uint32_t ifindex;
	uint8_t enabled;
	int ret;

	(void)vm;
	(void)nargs;
	ret = parse_ifindex(uc_fn_arg(0), &ifindex, ifname, sizeof(ifname));
	if (ret) {
		set_errno_error(ret, "resolve xPON interface");
		return NULL;
	}
	if (!parse_bool_arg(uc_fn_arg(1), &enabled)) {
		set_error(-EINVAL, "set_oam_enabled(): expected 0 or 1");
		return NULL;
	}

	ret = xpon_nl_open(&ctx);
	if (ret)
		goto fail_open;
	request_init(&ctx, &req, ctx.family_id, XPON_CMD_SET_OAM,
		     XPON_GENL_VERSION, 0);
	ret = add_u32(&req, XPON_ATTR_IFINDEX, ifindex);
	if (!ret)
		ret = add_nested_u8(&req, XPON_ATTR_OAM,
				    XPON_OAM_ATTR_ENABLED, enabled);
	if (!ret)
		ret = request_exec(&ctx, &req, false, NULL, NULL);
	xpon_nl_close(&ctx);
	if (ret) {
		set_errno_error(ret, "xPON OAM state change");
		return NULL;
	}

	set_error(0, "success");
	return ucv_boolean_new(true);

fail_open:
	set_errno_error(ret, "open xPON netlink");
	return NULL;
}

static const uc_function_list_t functions[] = {
	{ "error", uc_error },
	{ "family", uc_family },
	{ "ifindex", uc_ifindex },
	{ "status", uc_status },
	{ "set_mode", uc_set_mode },
	{ "set_oam_enabled", uc_set_oam_enabled },
};

void uc_module_init(uc_vm_t *vm, uc_value_t *scope)
{
	(void)vm;
	set_error(0, "success");
	uc_function_list_register(scope, functions);
	ucv_object_add(scope, "UAPI_VERSION", ucv_uint64_new(XPON_GENL_VERSION));
	ucv_object_add(scope, "MODE_GPON", ucv_uint64_new(XPON_MODE_GPON));
	ucv_object_add(scope, "MODE_EPON", ucv_uint64_new(XPON_MODE_EPON));
	ucv_object_add(scope, "MODE_XGSPON", ucv_uint64_new(XPON_MODE_XGSPON));
}
