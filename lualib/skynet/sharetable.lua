local skynet = require "skynet"
local core = require "skynet.sharetable.core"
local is_sharedtable = core.is_sharedtable
local mc = require "skynet.multicast"

local service
local channel_dispatch
local channel
skynet.init(function()
	service = skynet.uniqueservice "sharetabled"
	local channel = mc.new {
		channel =  skynet.call(service,"lua","channel"),
		dispatch = channel_dispatch,
	}
	channel:subscribe()
end)

local function report_close(t)
	local addr = rawget(t, "address")
	if addr then
		skynet.send(addr, "lua", "close")
	end
end

local sharetable = setmetatable ({address=service} , {
	__gc = report_close,
})

channel_dispatch=function(_channel, source, filename)
	sharetable.update(filename)
end

function sharetable.loadfile(filename, ...)
	skynet.call(service, "lua", "loadfile", filename, ...)
end

function sharetable.loadstring(filename, source, ...)
	skynet.call(service, "lua", "loadstring", filename, source, ...)
end

function sharetable.loadtable(filename, tbl)
	assert(type(tbl) == "table")
	skynet.call(service, "lua", "loadtable", filename, skynet.pack(tbl))
end

local RECORD = {}
function sharetable.query(filename)
	local newptr = skynet.call(service, "lua", "query", filename)
	if newptr then
		local t = core.clone(newptr)
		local map = RECORD[filename]
		if not map then
			map = {}
			RECORD[filename] = map
		end
		map[t] = true
		return t
	end
end


local pairs = pairs
local type = type
local assert = assert
local next = next
local rawset = rawset
local getuservalue = debug.getuservalue
local setuservalue = debug.setuservalue
local getupvalue = debug.getupvalue
local setupvalue = debug.setupvalue
local getlocal = debug.getlocal
local setlocal = debug.setlocal
local getinfo = debug.getinfo

local NILOBJ = {}
local function insert_replace(old_t, new_t, replace_map)
    for k, ov in pairs(old_t) do
        if type(ov) == "table" then
            local nv = new_t[k]
            if nv == nil then
                nv = NILOBJ
            end
            assert(replace_map[ov] == nil)
            replace_map[ov] = nv
            nv = type(nv) == "table" and nv or NILOBJ
            insert_replace(ov, nv, replace_map)
        end
    end
    replace_map[old_t] = new_t
    return replace_map
end


local function resolve_replace(replace_map)
    local match = {}
    local record_map = {}

    local function getnv(v)
        local nv = replace_map[v]
        if nv then
            if nv == NILOBJ then
                return nil
            end
            return nv
        end
        assert(false)
    end

    local function match_value(v)
        assert(v ~= nil)
        local tv = type(v)
        local f = match[tv]
        if record_map[v] or is_sharedtable(v) then
            return
        end

        if f then
            record_map[v] = true
            f(v)
        end
    end

    local function match_mt(v)
        local mt = getmetatable(v)
        if mt then
            local nv = replace_map[mt]
            if nv then
                nv = getnv(mt)
                setmetatable(v, nv)
            else
                match_value(mt)
            end
        end
    end

    local function match_table(t)
        for k,v in next, t do
            local nv = replace_map[v]
            if nv then
                nv = getnv(v)
                rawset(t, k, nv)
            else
                match_value(v)
            end
        end
        match_mt(t)
    end

    local function match_userdata(u)
        local uv = getuservalue(u)
        local nv = replace_map[uv]
        if nv then
            nv = getnv(uv)
            setuservalue(u, nv)
        end
        match_mt(u)
    end

    local function match_funcinfo(info)
        local func = info.func
        local nups = info.nups
        for i=1,nups do
            local name, upv = getupvalue(func, i)
            local nv = replace_map[upv]
            if nv then
                nv = getnv(upv)
                setupvalue(func, i, nv)
            elseif upv then
                match_value(upv)
            end
        end

        local level = info.level
        local curco = info.curco or coroutine.running()
        if not level then
            return
        end
        local i = 1
        while true do
            local name, v = getlocal(curco, level, i)
            if name == nil then
                break
            end
            if replace_map[v] then
                local nv = getnv(v)
                setlocal(curco, level, i, nv)
            elseif v then
                match_value(v)
            end
            i = i + 1
        end
    end

    local function match_function(f)
        local info = getinfo(f, "uf")
        match_funcinfo(info)
    end

    local function match_thread(co)
        local level = 1
        while true do
            local info = getinfo(co, level, "uf")
            if not info then
                break
            end
            info.level = level
            info.curco = co
            match_funcinfo(info)
            level = level + 1
        end
    end

    match["table"] = match_table
    match["function"] = match_function
    match["userdata"] = match_userdata
    match["thread"] = match_thread

    local root = debug.getregistry()
    assert(replace_map[root] == nil)
    match_table(root)
end

function sharetable.update(...)
	local names = {...}
	local replace_map = {}
	for _, name in ipairs(names) do
		local map = RECORD[name]
		if map then
			local new_t = sharetable.query(name)
			for old_t,_ in pairs(map) do
				if old_t ~= new_t then
					insert_replace(old_t, new_t, replace_map)
				end
			end
			RECORD[name] = nil
		end
	end

	resolve_replace(replace_map)
end


return sharetable

