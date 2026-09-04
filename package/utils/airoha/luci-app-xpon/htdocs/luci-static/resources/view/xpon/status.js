'use strict';
'require view';
'require poll';
'require dom';
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

function rate(value) {
	var n = Number(value);
	if (!Number.isFinite(n))
		return '-';
	if (n >= 1000000000)
		return (n / 1000000000).toFixed(3) + ' Gbit/s';
	if (n >= 1000000)
		return (n / 1000000).toFixed(3) + ' Mbit/s';
	return n + ' bit/s';
}

function row(label, value, id) {
	return E('tr', {}, [
		E('td', { 'class': 'td left', 'width': '42%' }, label),
		E('td', { 'class': 'td left', 'id': 'xpon-status-' + id }, text(value))
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
	var optical = status.optical || {};
	var oam = status.oam || {};
	return {
		interface: text(status.interface),
		ifindex: text(status.ifindex),
		mode: text(status.mode_name),
		modes: (status.available_mode_names || []).join(', ') || '-',
		registration: text(status.registration_name),
		carrier: yesno(status.carrier),
		signal: yesno(status.signal_detect),
		los: yesno(status.los),
		uapi: '%s / client %s (%s)'.format(text(status.uapi_version), text(status.client_uapi_version), status.uapi_compatible ? _('compatible') : _('mismatch')),
		oam_present: yesno(oam.present),
		oam_enabled: oam.present ? yesno(oam.enabled) : _('Unavailable'),
		optical_vendor: text(optical.vendor),
		optical_vendor_oui: text(optical.vendor_oui),
		optical_model: text(optical.model),
		optical_serial: text(optical.serial),
		optical_type: text(optical.type),
		optical_protocol: text(optical.protocol_name),
		optical_protocols: (optical.supported_protocol_names || []).join(', ') || '-',
		optical_capabilities: hex(optical.capabilities, 8),
		optical_tx_rate: rate(optical.tx_rate),
		optical_rx_rate: rate(optical.rx_rate),
		optical_present: yesno(optical.present),
		optical_ready: yesno(optical.ready),
		optical_rx_los: yesno(optical.rx_los),
		optical_tx_fault: yesno(optical.tx_fault),
		optical_tx_enabled: yesno(optical.tx_enabled),
		optical_temperature: temperature(optical.temperature_mc),
		optical_voltage: optical.voltage_uv == null ? '-' : optical.voltage_uv + ' µV',
		optical_bias: optical.bias_ua == null ? '-' : optical.bias_ua + ' µA',
		optical_tx_power: power(optical.tx_power_nw),
		optical_rx_power: power(optical.rx_power_nw),
		optical_alarms: hex(optical.alarms, 8)
	};
}

function update(status) {
	var v = values(status);
	Object.keys(v).forEach(function(key) {
		var node = document.getElementById('xpon-status-' + key);
		if (node)
			dom.content(node, v[key]);
	});
}

return view.extend({
	load: function() {
		return L.resolveDefault(rpc.xponStatus(''), {});
	},

	render: function(status) {
		var v = values(status);
		poll.add(function() {
			return L.resolveDefault(rpc.xponStatus(''), {}).then(update);
		}, 2);

		return E([], [
			E('h2', {}, _('xPON status')),
			E('p', {}, _('Runtime state reported by the Linux xPON Generic Netlink family. OMCI-specific state is available in the OMCI tab.')),
			section(_('xPON core'), [
				row(_('Interface'), v.interface, 'interface'), row(_('Interface index'), v.ifindex, 'ifindex'),
				row(_('Current mode'), v.mode, 'mode'), row(_('Available modes'), v.modes, 'modes'),
				row(_('Registration state'), v.registration, 'registration'), row(_('Carrier'), v.carrier, 'carrier'),
				row(_('Signal detect'), v.signal, 'signal'), row(_('Loss of signal'), v.los, 'los'),
				row(_('xPON UAPI'), v.uapi, 'uapi')
			]),
			section(_('Control-plane subsystems'), [
				row(_('EPON OAM provider present'), v.oam_present, 'oam_present'),
				row(_('EPON OAM administratively enabled'), v.oam_enabled, 'oam_enabled')
			]),
			section(_('Optical frontend'), [
				row(_('Vendor'), v.optical_vendor, 'optical_vendor'), row(_('Vendor OUI'), v.optical_vendor_oui, 'optical_vendor_oui'),
				row(_('Model'), v.optical_model, 'optical_model'), row(_('Serial'), v.optical_serial, 'optical_serial'),
				row(_('Type'), v.optical_type, 'optical_type'), row(_('Protocol'), v.optical_protocol, 'optical_protocol'),
				row(_('Supported protocols'), v.optical_protocols, 'optical_protocols'), row(_('Capabilities'), v.optical_capabilities, 'optical_capabilities'),
				row(_('TX line rate'), v.optical_tx_rate, 'optical_tx_rate'), row(_('RX line rate'), v.optical_rx_rate, 'optical_rx_rate'),
				row(_('Present'), v.optical_present, 'optical_present'), row(_('Ready'), v.optical_ready, 'optical_ready'),
				row(_('RX LOS'), v.optical_rx_los, 'optical_rx_los'), row(_('TX fault'), v.optical_tx_fault, 'optical_tx_fault'),
				row(_('TX enabled'), v.optical_tx_enabled, 'optical_tx_enabled'), row(_('Temperature'), v.optical_temperature, 'optical_temperature'),
				row(_('Voltage'), v.optical_voltage, 'optical_voltage'), row(_('Bias current'), v.optical_bias, 'optical_bias'),
				row(_('TX optical power'), v.optical_tx_power, 'optical_tx_power'), row(_('RX optical power'), v.optical_rx_power, 'optical_rx_power'),
				row(_('Alarms'), v.optical_alarms, 'optical_alarms')
			])
		]);
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
