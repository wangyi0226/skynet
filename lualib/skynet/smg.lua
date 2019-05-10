local skynet = require "skynet"
local smg_interface = require "smg.interface"

local smg = {}
local typeclass = {}

local interface_g = skynet.getenv("smg_interface_g")
local G = interface_g and require (interface_g) or { require = function() end }
interface_g = nil

skynet.register_protocol {
	name = "smg",
	id = skynet.PTYPE_SMG,
	pack = skynet.pack,
	unpack = skynet.unpack,
}

skynet.register_protocol {
	name = "smg_hotfix",
	id = skynet.PTYPE_SMG_HOTFIX,
	pack = skynet.pack,
	unpack = skynet.unpack,
}

function smg.interface(name)
	if typeclass[name] then
		return typeclass[name]
	end
	local si = smg_interface(name, G)

	local ret = {
		name = name,
		accept = {},
		response = {},
		system = {},
	}

	for _,v in ipairs(si) do
		local id, group, name, f = table.unpack(v)
		ret[group][name] = id
	end

	typeclass[name] = ret
	return ret
end

local meta = { __tostring = function(v) return string.format("[%s:%x]", v.type, v.handle) end}

local skynet_send = skynet.send
local skynet_call = skynet.call

local function gen_post(type, handle)
	return setmetatable({} , {
		__index = function( t, k )
			local id = type.accept[k]
			if not id then
				return function(...)
					skynet_send(handle, "smg_hotfix","accept",k, ...)
				end
			end
			return function(...)
				skynet_send(handle, "smg",id, ...)
			end
		end })
end

local function gen_req(type, handle)
	return setmetatable({} , {
		__index = function( t, k )
			local id = type.response[k]
			if not id then
				return function(...)
					return skynet_call(handle, "smg_hotfix","response",k, ...)
				end
			end
			return function(...)
				return skynet_call(handle, "smg", id, ...)
			end
		end })
end

local function wrapper(handle, name, type)
	return setmetatable ({
		post = gen_post(type, handle),
		req = gen_req(type, handle),
		type = name,
		handle = handle,
		}, meta)
end

local handle_cache = setmetatable( {} , { __mode = "kv" } )

function smg.rawnewservice(name, ...)
	local t = smg.interface(name)
	local handle = skynet.newservice("smgd", name)
	assert(handle_cache[handle] == nil)
	if t.system.init then
		skynet.call(handle, "smg", t.system.init, ...)
	end
	return handle
end

function smg.bind(handle, type)
	local ret = handle_cache[handle]
	if ret then
		assert(ret.type == type)
		return ret
	end
	local t = smg.interface(type)
	ret = wrapper(handle, type, t)
	handle_cache[handle] = ret
	return ret
end

function smg.newservice(name, ...)
	local handle = smg.rawnewservice(name, ...)
	return smg.bind(handle, name)
end

--注册时跟snaxd共用命名规则
function smg.uniqueservice(name, ...)
	local handle = assert(skynet.call(".service", "lua", "LAUNCH", "snaxd", name, ...))
	return smg.bind(handle, name)
end

function smg.uniqueservice_list(list)
	local ret={}
	for k,v in ipairs(list) do
		if type(v) == "string" then
			table.insert(ret,smg.uniqueservice(v))
		else
			assert(type(v)=="table")
			table.insert(ret,smg.uniqueservice(table.unpack(v)))
		end
	end
	return ret
end

function smg.globalservice(name, ...)
	local handle = assert(skynet.call(".service", "lua", "GLAUNCH", "snaxd", name, ...))
	return smg.bind(handle, name)
end

function smg.globalservice_list(list)
	local ret={}
	for k,v in ipairs(list) do
		if type(v) == "string" then
			table.insert(ret,smg.globalservice(v))
		else
			assert(type(v)=="table")
			table.insert(ret,smg.globalservice(table.unpack(v)))
		end
	end
	return ret
end

function smg.queryservice(name)
	local handle = assert(skynet.call(".service", "lua", "QUERY", "snaxd", name))
	return smg.bind(handle, name)
end

function smg.queryglobal(name)
	local handle = assert(skynet.call(".service", "lua", "GQUERY", "snaxd", name))
	return smg.bind(handle, name)
end

function smg.kill(obj, ...)
	local t = smg.interface(obj.type)
	skynet_call(obj.handle, "smg", t.system.exit, ...)
end

function smg.self()
	return smg.bind(skynet.self(), SERVICE_NAME)
end

function smg.exit(...)
	smg.kill(smg.self(), ...)
end

local function test_result(ok, ...)
	if ok then
		return ...
	else
		error(...)
	end
end

function smg.hotfix(obj, source, ...)
	local t = smg.interface(obj.type)
	return test_result(skynet_call(obj.handle, "smg", t.system.hotfix, source, ...))
end

function smg.printf(fmt, ...)
	skynet.error(string.format(fmt, ...))
end

function smg.profile_info(obj)
	local t = smg.interface(obj.type)
	return skynet_call(obj.handle, "smg", t.system.profile)
end

return smg
