'use strict';
'require view';
'require form';
'require uci';

return view.extend({
	load: function() {
		return uci.load('xpon');
	},

	render: function() {
		var m = new form.Map('xpon', _('xPON settings'),
			_('Common control-plane settings. Runtime changes are applied by rpcd through the Linux xPON and OMCI Generic Netlink families; the userspace package does not use sysfs.'));
		var s, o;

		s = m.section(form.TypedSection, 'core', _('xPON core'));
		s.anonymous = true;
		s.addremove = false;

		o = s.option(form.Value, 'device', _('xPON device'));
		o.default = 'pon0';
		o.datatype = 'uciname';
		o.description = _('Network device registered with the Linux xPON core.');

		o = s.option(form.ListValue, 'mode', _('Forced xPON mode'));
		o.value('', _('Kernel / driver default'));
		o.value('gpon', _('GPON'));
		o.value('epon', _('EPON'));
		o.value('xgspon', _('XGS-PON'));
		o.description = _('Changing the selected mode uses XPON_CMD_SET_MODE. The interface must satisfy the kernel requirements for runtime mode switching.');

		s = m.section(form.TypedSection, 'omci', _('OMCI subsystem'));
		s.anonymous = true;
		s.addremove = false;

		o = s.option(form.Value, 'device', _('xPON device'));
		o.default = 'pon0';
		o.datatype = 'uciname';
		o.description = _('Network device whose OMCI provider should be controlled.');

		o = s.option(form.Flag, 'enabled', _('Enable OMCI'));
		o.default = o.enabled;
		o.description = _('Enables the in-kernel OMCI agent for GPON and XGS-PON. OMCI is not applied while the selected xPON device is in EPON mode.');

		s = m.section(form.TypedSection, 'oam', _('EPON OAM subsystem'));
		s.anonymous = true;
		s.addremove = false;

		o = s.option(form.Value, 'device', _('xPON device'));
		o.default = 'pon0';
		o.datatype = 'uciname';
		o.description = _('Network device whose EPON OAM provider should be controlled.');

		o = s.option(form.Flag, 'enabled', _('Enable IEEE 802.3ah OAM'));
		o.default = o.enabled;
		o.description = _('Changes the administrative state of the OAM provider on the selected xPON device through Generic Netlink. If no OAM provider is registered yet, the value remains pending until one becomes available.');

		return m.render();
	}
});
