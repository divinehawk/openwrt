'use strict';
'require view';
'require form';
'require uci';

return view.extend({
	load: function() {
		return uci.load('xpon');
	},

	render: function() {
		var m = new form.Map('xpon', _('OMCI configuration'),
			_('Detailed ONU Management and Control Interface settings. Enable or disable the OMCI subsystem from the xPON Settings tab. Empty identity fields leave the value supplied by the kernel, device tree, NVMEM or driver unchanged.'));
		var s, o;

		s = m.section(form.TypedSection, 'omci', _('OMCI agent'));
		s.anonymous = true;
		s.addremove = false;

		o = s.option(form.Flag, 'permissive', _('Permissive mode'));
		o.default = o.enabled;
		o.description = _('Allow interoperability fallbacks for OLT behavior not covered by the strict path.');

		o = s.option(form.Flag, 'fake_omci', _('Fake unsupported OMCI responses'));
		o.default = o.disabled;

		o = s.option(form.Flag, 'dying_gasp', _('Dying gasp'));
		o.default = o.enabled;

		s = m.section(form.TypedSection, 'omci', _('ONU identity'));
		s.anonymous = true;
		s.addremove = false;

		o = s.option(form.Value, 'serial', _('GPON serial number'));
		o.datatype = 'maxlength(32)';
		o.placeholder = 'MSTC12345678';

		o = s.option(form.Value, 'vendor_id', _('Vendor ID'));
		o.datatype = 'maxlength(16)';
		o.placeholder = 'MSTC';

		o = s.option(form.Value, 'hardware_version', _('Hardware version'));
		o.datatype = 'maxlength(14)';

		o = s.option(form.Value, 'software_version_0', _('Software image 0 version'));
		o.datatype = 'maxlength(14)';

		o = s.option(form.Value, 'software_version_1', _('Software image 1 version'));
		o.datatype = 'maxlength(14)';

		o = s.option(form.Value, 'equipment_id', _('Equipment ID'));
		o.datatype = 'maxlength(20)';

		o = s.option(form.Value, 'password', _('SLID / GPON password'));
		o.password = true;
		o.datatype = 'maxlength(32)';

		o = s.option(form.Value, 'omcc_version', _('OMCC version'));
		o.placeholder = '0xA1';
		o.description = _('Decimal or hexadecimal byte value. Leave empty to retain the current kernel value.');

		o = s.option(form.Value, 'traffic_management_option', _('Traffic management option'));
		o.datatype = 'range(0,255)';

		o = s.option(form.ListValue, 'onu_type', _('ONU type'));
		o.value('', _('Kernel / driver default'));
		o.value('0', _('Other'));
		o.value('1', _('SFU - Single Family Unit'));
		o.value('2', _('HGU - Home Gateway Unit'));
		o.value('3', _('MDU - Multi-Dwelling Unit'));
		o.value('4', _('SBU - Single Business Unit'));
		o.value('5', _('MTU - Multi-Tenant Unit'));
		o.value('6', _('CBU - Cellular Backhaul Unit'));

		o = s.option(form.Value, 'uni_count', _('UNI count'));
		o.datatype = 'range(0,255)';

		return m.render();
	}
});
