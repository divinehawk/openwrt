'use strict';
'require view';
'require xpon.omci.mib as mib';

return view.extend({
	load: mib.load,
	render: function(data) {
		return mib.render('bridge', data, _('Bridge / UNI'), _('Ethernet UNI, bridge, VLAN and datapath managed entities.'));
	},
	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
