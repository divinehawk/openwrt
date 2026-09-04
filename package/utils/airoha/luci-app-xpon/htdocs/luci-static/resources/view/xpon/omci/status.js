'use strict';
'require view';
'require poll';
'require dom';
'require uci';
'require xpon.rpc as rpc';

function text(value, fallback) {
	return value == null || value === '' ? (fallback || '-') : String(value);
}

function yesno(value) {
	return value == null ? '-' : (value ? _('Yes') : _('No'));
}

function hex(value, width) {
	if (value == null)
		return '-';
	return '0x' + Number(value).toString(16).padStart(width || 2, '0');
}

function power(value) {
	var n = Number(value);
	if (!Number.isFinite(n) || n <= 0)
		return '-';
	return '%s nW (%s dBm)'.format(n, (10 * Math.log10(n / 1000000)).toFixed(2));
}

function temperature(value) {
	var n = Number(value);
	return Number.isFinite(n) ? (n / 1000).toFixed(3) + ' °C' : '-';
}

function row(label, value, id) {
	return E('tr', {}, [
		E('td', { 'class': 'td left', 'width': '42%' }, label),
		E('td', { 'class': 'td left', 'id': 'xpon-omci-status-' + id }, text(value))
	]);
}

function section(title, rows) {
	return E('div', { 'class': 'cbi-section' }, [
		E('h3', {}, title),
		E('table', { 'class': 'table' }, rows)
	]);
}

function values(status) {
	status = status || {};
	var identity = status.identity || {};
	var olt = status.olt || {};
	return {
		device_name: text(status.device_name),
		device_id: text(status.device),
		state: status.state == null ? '-' : 'O' + status.state,
		ifindex: text(status.ifindex),
		onu_id: text(status.onu_id),
		gem_port: text(status.gem_port),
		channel_up: yesno(status.channel_up),
		agent_enabled: yesno(status.agent_enabled),
		agent_operational: yesno(status.agent_operational),
		permissive: yesno(status.permissive),
		fake_omci: yesno(status.fake_omci),
		dying_gasp: yesno(status.dying_gasp),
		uapi: '%s / client %s (%s)'.format(text(status.uapi_version), text(status.client_uapi_version), status.uapi_compatible ? _('compatible') : _('mismatch')),
		capabilities: hex(status.capabilities, 8),
		mib_sync: text(status.mib_sync),
		mib_objects: text(status.mib_objects),
		rx_packets: text(status.rx_packets),
		rx_bytes: text(status.rx_bytes),
		rx_dropped: text(status.rx_dropped),
		tx_packets: text(status.tx_packets),
		tx_bytes: text(status.tx_bytes),
		tx_errors: text(status.tx_errors),
		responses: text(status.responses),
		duplicates: text(status.duplicates),
		unsupported: text(status.unsupported),
		fake_responses: text(status.fake_responses),
		serial: text(identity.serial),
		vendor_id: text(identity['vendor-id']),
		hardware_version: text(identity['hardware-version']),
		software_version_0: text(identity['software-version-0']),
		software_version_1: text(identity['software-version-1']),
		equipment_id: text(identity['equipment-id']),
		onu_type: text(identity['onu-type-name']),
		omcc_version: identity['omcc-version'] == null ? '-' : '%s (%s)'.format(hex(identity['omcc-version'], 2), identity['omcc-version']),
		olt_vendor: text(olt.vendor_id || olt.vendor),
		olt_equipment: text(olt.equipment_id || olt.model),
		olt_version: text(olt.version),
		fec_downstream: text(status.fec_downstream),
		fec_upstream: text(status.fec_upstream),
		bosa_temperature: temperature(status.bosa_temperature_mc),
		bosa_voltage: status.bosa_voltage_uv == null ? '-' : status.bosa_voltage_uv + ' µV',
		bosa_bias: status.bosa_bias_ua == null ? '-' : status.bosa_bias_ua + ' µA',
		bosa_tx_power: power(status.bosa_tx_power_nw),
		bosa_rx_power: power(status.bosa_rx_power_nw),
		bosa_alarms: hex(status.bosa_alarms, 8)
	};
}

function update(status) {
	var v = values(status);
	Object.keys(v).forEach(function(key) {
		var node = document.getElementById('xpon-omci-status-' + key);
		if (node)
			dom.content(node, v[key]);
	});
}

function configuredDevice() {
	var section = (uci.sections('xpon', 'omci') || [])[0] || {};
	return section.device || 'pon0';
}

return view.extend({
	load: function() {
		return uci.load('xpon').then(function() {
			return L.resolveDefault(rpc.omciStatus(configuredDevice()), {});
		});
	},

	render: function(status) {
		var v = values(status);
		poll.add(function() {
			return L.resolveDefault(rpc.omciStatus(configuredDevice()), {}).then(update);
		}, 2);

		return E([], [
			E('h2', {}, _('OMCI status')),
			E('p', {}, _('Runtime state reported by the Linux OMCI Generic Netlink family.')),
			section(_('OMCI channel and agent'), [
				row(_('xPON device'), v.device_name, 'device_name'), row(_('OMCI device ID'), v.device_id, 'device_id'),
				row(_('OMCI state'), v.state, 'state'), row(_('Interface index'), v.ifindex, 'ifindex'),
				row(_('ONU ID'), v.onu_id, 'onu_id'), row(_('Default GEM port'), v.gem_port, 'gem_port'),
				row(_('OMCI channel up'), v.channel_up, 'channel_up'), row(_('Agent enabled'), v.agent_enabled, 'agent_enabled'),
				row(_('Agent operational'), v.agent_operational, 'agent_operational'), row(_('Permissive mode'), v.permissive, 'permissive'),
				row(_('Fake OMCI'), v.fake_omci, 'fake_omci'), row(_('Dying gasp'), v.dying_gasp, 'dying_gasp'),
				row(_('OMCI UAPI'), v.uapi, 'uapi'), row(_('Capabilities'), v.capabilities, 'capabilities')
			]),
			section(_('ONU identity'), [
				row(_('Serial number'), v.serial, 'serial'), row(_('Vendor ID'), v.vendor_id, 'vendor_id'), row(_('ONU type'), v.onu_type, 'onu_type'),
				row(_('Hardware version'), v.hardware_version, 'hardware_version'), row(_('Software image 0 version'), v.software_version_0, 'software_version_0'),
				row(_('Software image 1 version'), v.software_version_1, 'software_version_1'), row(_('Equipment ID'), v.equipment_id, 'equipment_id'),
				row(_('OMCC version'), v.omcc_version, 'omcc_version')
			]),
			section(_('OLT information'), [
				row(_('OLT vendor ID'), v.olt_vendor, 'olt_vendor'), row(_('OLT equipment ID'), v.olt_equipment, 'olt_equipment'),
				row(_('OLT version'), v.olt_version, 'olt_version')
			]),
			section(_('MIB and OMCI traffic'), [
				row(_('MIB data sync'), v.mib_sync, 'mib_sync'), row(_('MIB objects'), v.mib_objects, 'mib_objects'),
				row(_('RX packets'), v.rx_packets, 'rx_packets'), row(_('RX bytes'), v.rx_bytes, 'rx_bytes'), row(_('RX dropped'), v.rx_dropped, 'rx_dropped'),
				row(_('TX packets'), v.tx_packets, 'tx_packets'), row(_('TX bytes'), v.tx_bytes, 'tx_bytes'), row(_('TX errors'), v.tx_errors, 'tx_errors'),
				row(_('Agent responses'), v.responses, 'responses'), row(_('Duplicates'), v.duplicates, 'duplicates'),
				row(_('Unsupported requests'), v.unsupported, 'unsupported'), row(_('Fake responses'), v.fake_responses, 'fake_responses')
			]),
			section(_('OMCI optical telemetry'), [
				row(_('Downstream FEC'), v.fec_downstream, 'fec_downstream'), row(_('Upstream FEC'), v.fec_upstream, 'fec_upstream'),
				row(_('BOSA temperature'), v.bosa_temperature, 'bosa_temperature'), row(_('BOSA voltage'), v.bosa_voltage, 'bosa_voltage'),
				row(_('BOSA bias current'), v.bosa_bias, 'bosa_bias'), row(_('TX optical power'), v.bosa_tx_power, 'bosa_tx_power'),
				row(_('RX optical power'), v.bosa_rx_power, 'bosa_rx_power'), row(_('BOSA alarms'), v.bosa_alarms, 'bosa_alarms')
			])
		]);
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
