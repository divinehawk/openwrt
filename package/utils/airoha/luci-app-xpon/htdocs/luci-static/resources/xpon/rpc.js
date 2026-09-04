'use strict';
'require baseclass';
'require rpc';

var xponObject = 'xpon';
var omciObject = 'xpon.omci';

function declare(object, method, params, expect) {
	return rpc.declare({
		object: object,
		method: method,
		params: params || [],
		expect: expect || {}
	});
}

return baseclass.extend({
	xponFamily: declare(xponObject, 'family', [], { family: {} }),
	xponStatus: declare(xponObject, 'status', [ 'interface' ], { status: {} }),
	xponModeSet: declare(xponObject, 'mode_set', [ 'interface', 'mode' ], {}),
	xponOamSet: declare(xponObject, 'oam_set', [ 'interface', 'enabled' ], {}),
	xponApply: declare(xponObject, 'apply', [], {}),

	omciFamily: declare(omciObject, 'family', [], { family: {} }),
	omciStatus: declare(omciObject, 'status', [ 'device' ], { status: {} }),
	omciAgentSet: declare(omciObject, 'agent_set', [ 'device', 'enabled', 'permissive', 'fake_omci', 'dying_gasp' ], {}),
	omciConfigList: declare(omciObject, 'config_list', [ 'device' ], { config: {} }),
	omciConfigDetails: declare(omciObject, 'config_details', [ 'device' ], { details: [] }),
	omciConfigGet: declare(omciObject, 'config_get', [ 'device', 'key' ], { config: {} }),
	omciConfigInspect: declare(omciObject, 'config_inspect', [ 'device', 'key' ], { config: {} }),
	omciConfigSet: declare(omciObject, 'config_set', [ 'device', 'key', 'value' ], {}),
	omciClassList: declare(omciObject, 'class_list', [ 'device' ], { classes: [] }),
	omciClassGet: declare(omciObject, 'class_get', [ 'device', 'class_id' ], { class: {} }),
	omciMibList: declare(omciObject, 'mib_list', [ 'device' ], { objects: [] }),
	omciMibGet: declare(omciObject, 'mib_get', [ 'device', 'class_id', 'entity_id' ], { object: {} }),
	omciMibSet: declare(omciObject, 'mib_set', [ 'device', 'class_id', 'entity_id', 'attr_mask', 'data' ], {}),
	omciMibDelete: declare(omciObject, 'mib_delete', [ 'device', 'class_id', 'entity_id' ], {}),
	omciMibReset: declare(omciObject, 'mib_reset', [ 'device' ], {}),
	omciRawTx: declare(omciObject, 'raw_tx', [ 'device', 'pdu' ], {}),
	omciApply: declare(omciObject, 'apply', [], {})
});
