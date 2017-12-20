#pragma once
#include <iostream>

#include "open62541.h"

#define SOL_CHECK_ARGUMENTS 1
#include "sol/sol.hpp"

#include "opcua_interfaces.hpp"

namespace lua_opcua {
extern std::string toString(const UA_NodeId& id);

UA_StatusCode UA_Node_IteratorCallback(UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId, void* handle);

class UA_Node;
struct UA_Node_callback {
	const UA_Node* _parent;

	UA_Node_callback(const UA_Node* parent) : _parent(parent) {}
	virtual ~UA_Node_callback(){};
	virtual UA_StatusCode operator()(UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId) = 0;
};
struct UA_Node_Iter : public UA_Node_callback {
	std::vector<UA_Node> _childs;

	UA_Node_Iter(const UA_Node* parent) : UA_Node_callback(parent) {}
	UA_StatusCode operator()(UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId);
};

struct UA_Node_Finder : public UA_Node_callback {
	UA_String _name;
	int _ns;
	AttributeReader* _reader;
	std::vector<UA_Node> _nodes;
	bool _found;

	UA_Node_Finder(const UA_Node* parent, const std::string& name, AttributeReader* reader);
	~UA_Node_Finder() { UA_String_deleteMembers(&_name); }
	UA_StatusCode operator()(UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId);
};

#define MAP_NODE_PROPERTY(PT, PN) \
	PT get##PN() const { \
		PT val; PT##_init(&val); \
		auto reader = _mgr->getAttributeReader(); \
		UA_StatusCode re = reader->read##PN(_id, &val); \
		return val; \
	} \
	UA_StatusCode set##PN(const PT& val) const { \
		auto writer = _mgr->getAttributeWriter(); \
		auto re = writer->write##PN(_id, &val); \
		if (re != UA_STATUSCODE_GOOD) { \
			std::cout << "Write Property to :" << toString(_id) << " err_code: " << UA_StatusCode_name(re) << std::endl; \
		} \
		return re; \
	}

//#define SOL_MAP_NODE_PROPERTY(LN, DN) #LN, sol::property(&UA_Node::get##DN, &UA_Node::set##DN)
#define SOL_MAP_NODE_PROPERTY(LN, DN) "get"#DN, &UA_Node::get##DN, "set"#DN, &UA_Node::set##DN


struct AutoReleaseNodeId {
	UA_NodeId id;
	AutoReleaseNodeId() {
		UA_NodeId_init(&id);
	}
	~AutoReleaseNodeId() {
		UA_NodeId_deleteMembers(&id);
	}
	operator UA_NodeId* () {
		return &id;
	}
};

class UA_Node {
protected:
	friend class UA_Client_Proxy;
	friend class UA_Server_Proxy;
	friend class UA_Node_Iter;
	friend class UA_Node_Finder;
	UA_Node(NodeMgr* mgr, const UA_NodeId id, const UA_NodeId referenceType, UA_NodeClass node_class) : _mgr(mgr), _class(node_class) {
		UA_NodeId_copy(&id, &_id);
		UA_NodeId_copy(&referenceType, &_referenceType);
	}
public:
	NodeMgr* _mgr;
	UA_NodeId _id;
	UA_NodeId _referenceType;
	UA_NodeClass _class;

	~UA_Node() {
		UA_NodeId_deleteMembers(&_id);
		UA_NodeId_deleteMembers(&_referenceType);
	}
	UA_Node(const UA_Node& obj) {
		_mgr = obj._mgr;
		UA_NodeId_copy(&obj._id, &_id);
		UA_NodeId_copy(&obj._referenceType, &_referenceType);
		_class = obj._class;
	}

	operator std::string() {
		std::stringstream ss;
		ss << "Node(id=" << toString(_id) << ";type=" << toString(_referenceType) << ";class=" << _class << ")";
		return ss.str();
	}

	sol::variadic_results callMethod(const UA_NodeId methodId, sol::as_table_t<std::vector<UA_Variant>> inputs, sol::this_state L) {
		std::vector<UA_Variant> result_vec;
		result_vec.resize(100);
		UA_Variant* p = &result_vec[0];
		size_t out = 100;
		UA_StatusCode re = _mgr->callMethod(_id, methodId, inputs.source.size(), &inputs.source[0], &out, &p);
		result_vec.resize(out);
		/*
		 * RETURN_RESULT(sol::as_returns<UA_Variant>, sol::as_returns(std::move(result_vec)))
		 */
		RETURN_RESULT(std::vector<UA_Variant>, result_vec)
	}

	UA_StatusCode deleteNode(bool deleteReferences) {
		return _mgr->deleteNode(_id, deleteReferences);
	}
	sol::variadic_results addFolder(const UA_NodeId id, const char* browse, const UA_ObjectAttributes attr, sol::this_state L) {
		UA_QualifiedName browse_name = UA_QUALIFIEDNAME_ALLOC(id.namespaceIndex, browse);
		UA_NodeId referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
		UA_NodeId type = UA_NODEID_NUMERIC(0, UA_NS0ID_FOLDERTYPE);
		AutoReleaseNodeId outId;
		UA_StatusCode re = _mgr->addObject(id, _id, referenceTypeId, browse_name, type, attr, outId);
		UA_QualifiedName_deleteMembers(&browse_name);
		UA_NodeId_deleteMembers(&referenceTypeId);
		UA_NodeId_deleteMembers(&type);
		RETURN_RESULT(UA_Node, UA_Node(_mgr, *outId, _referenceType, UA_NODECLASS_OBJECT))
	}
	sol::variadic_results addObject(const UA_NodeId id, const char* browse, const UA_ObjectAttributes attr, sol::this_state L) {
		UA_QualifiedName browse_name = UA_QUALIFIEDNAME_ALLOC(id.namespaceIndex, browse);
		UA_NodeId referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
		UA_NodeId type = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE);
		AutoReleaseNodeId outId;
		UA_StatusCode re = _mgr->addObject(id, _id, referenceTypeId, browse_name, type, attr, outId);
		UA_QualifiedName_deleteMembers(&browse_name);
		UA_NodeId_deleteMembers(&referenceTypeId);
		UA_NodeId_deleteMembers(&type);
		RETURN_RESULT(UA_Node, UA_Node(_mgr, *outId, _referenceType, UA_NODECLASS_OBJECT))
	}
	sol::variadic_results addVariable(const UA_NodeId id, const char* browse, const UA_VariableAttributes attr, sol::this_state L) {
		UA_QualifiedName browse_name = UA_QUALIFIEDNAME_ALLOC(id.namespaceIndex, browse);
		UA_NodeId referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
		AutoReleaseNodeId outId;
		UA_StatusCode re = _mgr->addVariable(id, _id, referenceTypeId, browse_name, UA_NODEID_NULL, attr, outId);
		UA_QualifiedName_deleteMembers(&browse_name);
		UA_NodeId_deleteMembers(&referenceTypeId);
		RETURN_RESULT(UA_Node, UA_Node(_mgr, *outId, _referenceType, UA_NODECLASS_VARIABLE))
	}
	sol::variadic_results addView(const UA_NodeId id, const char* browse, const UA_ViewAttributes attr, sol::this_state L) {
		UA_QualifiedName browse_name = UA_QUALIFIEDNAME_ALLOC(id.namespaceIndex, browse);
		UA_NodeId referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
		AutoReleaseNodeId outId;
		UA_StatusCode re = _mgr->addView(id, _id, referenceTypeId, browse_name, attr, outId);
		UA_QualifiedName_deleteMembers(&browse_name);
		UA_NodeId_deleteMembers(&referenceTypeId);
		RETURN_RESULT(UA_Node, UA_Node(_mgr, *outId, _referenceType, UA_NODECLASS_VIEW))
	}
	sol::variadic_results addMethod(const UA_NodeId id, const char* browse, const UA_MethodAttributes attr, sol::this_state L) {
		UA_QualifiedName browse_name = UA_QUALIFIEDNAME_ALLOC(id.namespaceIndex, browse);
		UA_NodeId referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
		AutoReleaseNodeId outId;
		UA_StatusCode re = _mgr->addMethod(id, _id, referenceTypeId, browse_name, attr, NULL, 0, NULL, 0, NULL, NULL, outId);
		UA_QualifiedName_deleteMembers(&browse_name);
		UA_NodeId_deleteMembers(&referenceTypeId);
		RETURN_RESULT(UA_Node, UA_Node(_mgr, *outId, _referenceType, UA_NODECLASS_METHOD))
	}
	sol::variadic_results addReference(const UA_ExpandedNodeId id, bool isForward, const char* uri, UA_NodeClass node_class, sol::this_state L) {
		UA_NodeId referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
		UA_String targetServerUri = UA_STRING_ALLOC(uri);
		UA_StatusCode re = _mgr->addReference(_id, referenceTypeId, id, isForward, targetServerUri, node_class);
		UA_NodeId_deleteMembers(&referenceTypeId);
		UA_String_deleteMembers(&targetServerUri);
		RETURN_RESULT(bool, true)
	}
	sol::variadic_results deleteReference(const UA_ExpandedNodeId id, bool isForward, UA_NodeClass node_class, sol::this_state L) {
		UA_NodeId referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
		UA_StatusCode re = _mgr->deleteReference(_id, referenceTypeId, id, isForward, node_class);
		UA_NodeId_deleteMembers(&referenceTypeId);
		RETURN_RESULT(bool, true)
	}

	std::vector<UA_Node> getChildren() const {
		UA_Node_Iter iter(this);
		UA_StatusCode re = _mgr->forEachChildNodeCall(_id, UA_Node_IteratorCallback, &iter);
		return iter._childs;
	}
	bool _getChild(const std::string& name, std::vector<UA_Node>& result) {
		UA_Node_Finder op(this, name, _mgr->getAttributeReader());
		UA_StatusCode re = _mgr->forEachChildNodeCall(_id, UA_Node_IteratorCallback, &op);
		if (op._found)
			result.insert(result.end(), op._nodes.begin(), op._nodes.end());
		return op._found;
	}
	sol::variadic_results getChild(const std::string& name, sol::this_state L) {
		//std::cout << "getChild in name" << std::endl;
		sol::variadic_results result;
		std::vector<UA_Node> nodes;
		bool found = _getChild(name, nodes);
		if (found) { 
			for(auto node : nodes) {
				result.push_back({ L, sol::in_place_type<UA_Node>, node});
			}
		} else {
			result.push_back({ L, sol::in_place_type<sol::lua_nil_t>, sol::lua_nil_t()});
			result.push_back({ L, sol::in_place_type<std::string>, std::string("Not found!")});
		}
		return result;
	}
	void _getChild(std::vector<UA_Node>& nodes, const std::string& name, std::vector<UA_Node>& results) {
		for(auto parent : nodes) {
			parent._getChild(name, results);
		}
	}
	sol::variadic_results getChild(const sol::as_table_t<std::vector<std::string> > names, sol::this_state L) {
		//std::cout << "getChild in vector" << std::endl;
		sol::variadic_results result;
		auto ptr = names.source.begin();

		std::vector<UA_Node> results;
		results.push_back(*this);
		for(; ptr != names.source.end(); ++ptr) {
			std::string name = *ptr;
			std::vector<UA_Node> founds; 
			_getChild(results, name, founds);
			if (founds.empty())
			{
				result.push_back({ L, sol::in_place_type<sol::lua_nil_t>, sol::lua_nil_t()});
				result.push_back({ L, sol::in_place_type<std::string>, name + " Not found!"});
				return result;
			}
			founds.swap(results);
		}
		for (auto node : results)
			result.push_back({ L, sol::in_place_type<UA_Node>, node});
		return result;
	}

	/*
	const std::string getBrowseName() const {
		auto reader = _mgr->getAttributeReader();
		UA_QualifiedName browse_name; UA_QualifiedName_init(&browse_name);
		UA_StatusCode re = reader->readBrowseName(_id, &browse_name);
		if (re == UA_STATUSCODE_GOOD) {
			return std::string((char*)browse_name.name.data, browse_name.name.length);
		} else {
			return std::string(UA_StatusCode_name(re));
		}
	}
	void setBrowseName(const std::string name) {
		auto writer = _mgr->getAttributeWriter();
		UA_QualifiedName browse_name = UA_QUALIFIEDNAME_ALLOC(_id.namespaceIndex, name.c_str());
		writer->writeBrowseName(_id, &browse_name);
		UA_QualifiedName_deleteMembers(&browse_name);
	}
	UA_DataValue getDataValue() const {
		UA_DataValue val; UA_DataValue_init(&val);
		auto reader = _mgr->getAttributeReader();
		UA_StatusCode re = reader->readDataValue(_id, &val);
		return val;
	}
	UA_StatusCode setDataValue(const UA_DataValue& val) const {
		auto writer = _mgr->getAttributeWriter();
		return writer->writeDataValue(_id, &val);
	}
	*/
	MAP_NODE_PROPERTY(UA_NodeClass, NodeClass)
	MAP_NODE_PROPERTY(UA_QualifiedName, BrowseName)
	MAP_NODE_PROPERTY(UA_LocalizedText, DisplayName)
	MAP_NODE_PROPERTY(UA_LocalizedText, Description)
	MAP_NODE_PROPERTY(UA_UInt32, WriteMask)
	MAP_NODE_PROPERTY(UA_UInt32, UserWriteMask)
	MAP_NODE_PROPERTY(UA_Boolean, IsAbstract)
	MAP_NODE_PROPERTY(UA_Boolean, Symmetric)
	MAP_NODE_PROPERTY(UA_LocalizedText, InverseName)
	MAP_NODE_PROPERTY(UA_Boolean, ContainsNoLoops)
	MAP_NODE_PROPERTY(UA_Byte, EventNotifier)
	MAP_NODE_PROPERTY(UA_Variant, Value)
	MAP_NODE_PROPERTY(UA_DataValue, DataValue)
	MAP_NODE_PROPERTY(UA_NodeId, DataType)
	MAP_NODE_PROPERTY(UA_Int32, ValueRank)
	// ArrayDimensions TODO:
	MAP_NODE_PROPERTY(UA_Byte, AccessLevel)
	MAP_NODE_PROPERTY(UA_Byte, UserAccessLevel)
	MAP_NODE_PROPERTY(UA_Double, MinimumSamplingInterval)
	MAP_NODE_PROPERTY(UA_Boolean, Historizing)
	MAP_NODE_PROPERTY(UA_Boolean, Executable)
	MAP_NODE_PROPERTY(UA_Boolean, UserExecutable)
};

UA_StatusCode UA_Node_Iter::operator()(UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId) {
	UA_Node node(_parent->_mgr, childId, referenceTypeId, UA_NODECLASS_OBJECT);
	_childs.push_back(node);
}

UA_Node_Finder::UA_Node_Finder(const UA_Node* parent, const std::string& name, AttributeReader* reader)
	: UA_Node_callback(parent), _reader(reader), _found(false)
{
	auto index = name.find(":");
	if (index != name.npos) {
		std::stringstream ss;
		ss << name.substr(0, index);
		ss >> _ns;
		//std::cout << "autoQualifiedName ns:" << _ns << " name:" << name.substr(index+1) << std::endl;
		_name = UA_STRING_ALLOC(name.substr(index + 1).c_str());
	} else {
		_ns = _parent->_id.namespaceIndex;
		_name = UA_STRING_ALLOC(name.c_str());
	}
}

UA_StatusCode UA_Node_Finder::operator()(UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId) {
	if (childId.namespaceIndex != _ns)
		return UA_STATUSCODE_GOOD;

	UA_Node node(_parent->_mgr, childId, referenceTypeId, UA_NODECLASS_OBJECT);
	UA_QualifiedName browse_name; UA_QualifiedName_init(&browse_name);
	UA_StatusCode re = _reader->readBrowseName(childId, &browse_name);
	//std::cout << "UA_Node_Finder ns:" << browse_name.namespaceIndex << "," << childId.namespaceIndex << " name:" << (char*)browse_name.name.data << std::endl;
	if (re == UA_STATUSCODE_GOOD && UA_String_equal(&browse_name.name, &_name)) {
		_nodes.push_back(node);
		_found = true;
	}
	return UA_STATUSCODE_GOOD;
}

UA_StatusCode UA_Node_IteratorCallback(UA_NodeId childId, UA_Boolean isInverse, UA_NodeId referenceTypeId, void* handle) {
	UA_Node_callback* cb = (UA_Node_callback*)handle;
	return (*cb)(childId, isInverse, referenceTypeId);
}

void reg_opcua_node(sol::table& module) {
	module.new_usertype<UA_Node>("Node",
		"new", sol::no_constructor,
		"id", sol::readonly(&UA_Node::_id),
		"__tostring", [](UA_Node& node) { return (std::string)node; },
		"typeClass", &UA_Node::_class,
		"nodeMgr", &UA_Node::_mgr,
		"deleteNode", &UA_Node::deleteNode,
		"addFolder", &UA_Node::addFolder,
		"addObject", &UA_Node::addObject,
		"addVariable", &UA_Node::addVariable,
		"addView", &UA_Node::addView,
		"addReference", &UA_Node::addReference,
		"deleteReference", &UA_Node::deleteReference,
		"getChildren", &UA_Node::getChildren,
		"getChild", sol::overload(
			static_cast<sol::variadic_results (UA_Node::*)(const std::string& name, sol::this_state L) >(&UA_Node::getChild),
			static_cast<sol::variadic_results (UA_Node::*)(const sol::as_table_t<std::vector<std::string> > names, sol::this_state L) >(&UA_Node::getChild)
		),
		SOL_MAP_NODE_PROPERTY(nodeClass, NodeClass),
		SOL_MAP_NODE_PROPERTY(browseName, BrowseName),
		SOL_MAP_NODE_PROPERTY(displayName, DisplayName),
		SOL_MAP_NODE_PROPERTY(description, Description),
		SOL_MAP_NODE_PROPERTY(writeMask, WriteMask),
		SOL_MAP_NODE_PROPERTY(userWriteMask, UserWriteMask),
		SOL_MAP_NODE_PROPERTY(isAbstract, IsAbstract),
		SOL_MAP_NODE_PROPERTY(symetric, Symmetric),
		SOL_MAP_NODE_PROPERTY(inverseName, InverseName),
		SOL_MAP_NODE_PROPERTY(containsNoLoops, ContainsNoLoops),
		SOL_MAP_NODE_PROPERTY(eventNotifier, EventNotifier),
		SOL_MAP_NODE_PROPERTY(value, Value),
		SOL_MAP_NODE_PROPERTY(dataValue, DataValue),
		SOL_MAP_NODE_PROPERTY(dataType, DataType),
		SOL_MAP_NODE_PROPERTY(valueTank, ValueRank),
		// ArrayDimensions TODO:
		SOL_MAP_NODE_PROPERTY(accessLevel, AccessLevel),
		SOL_MAP_NODE_PROPERTY(userAccessLevel, UserAccessLevel),
		SOL_MAP_NODE_PROPERTY(minimumSamplingInterval, MinimumSamplingInterval),
		SOL_MAP_NODE_PROPERTY(historizing, Historizing),
		SOL_MAP_NODE_PROPERTY(executable, Executable),
		SOL_MAP_NODE_PROPERTY(userExecutable, UserExecutable)
	);
}

}