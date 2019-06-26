local skynet = require "skynet"
local c = require "skynet.core"
local smg_interface = require "smg.interface"
local profile = require "skynet.profile"
local smg = require "skynet.smg"
require "skynet.manager"

local smg_name = tostring(...)
local loaderpath = skynet.getenv"smg_loader"
local loader = loaderpath and assert(dofile(loaderpath))
local func, pattern,smg_env = smg_interface(smg_name, _ENV, loader)
local smg_path = pattern:sub(1,pattern:find("?", 1, true)-1) .. smg_name ..  "/"
package.path = smg_path .. "?.lua;" .. package.path

SERVICE_NAME = smg_name
SERVICE_PATH = smg_path
_ENV.hotfix_func={}

local function hotfix_func_id(tbl,group)
	local tmp = {}
	hotfix_func[group]={}
	local function newindex(t, name, f)
		if type(name) ~= "string" then
			error (string.format("%s method only support string", group))
		end
		if type(f) ~= "function" then
			error (string.format("%s.%s must be function", group, name))
		end
		if tmp[name] then
			error (string.format("%s.%s duplicate definition", group, name))
		end
		for id,method in ipairs(func) do
			if method[2] == group and method[3] == name then
				error (string.format("%s.%s duplicate definition", group, name))
				break
			end
		end
		tmp[name] = true
		hotfix_func[group][name]={0,group,name,f}
		rawset(t,name,f)
	end
	local function index(t, name, f)
		return tbl[name]
	end
	return setmetatable({}, { __newindex = newindex, __index=index})
end

_ENV.accept=hotfix_func_id(smg_env.accept, "accept") 
_ENV.response=hotfix_func_id(smg_env.response,"response")
_ENV.dft_dispatcher=false
_ENV.enablecluster=false
_ENV.func=func
_ENV.sublist=false
_ENV.subid=false
_ENV.subsrv_name=false
_ENV.__patch=function(f1,f2)
	local i = 1
	local uv={}
	while true do
		local name, value = debug.getupvalue(f2, i)
		print("find:",name,value,i)
		if name == nil then
			break
		end
		uv[name]=i
		i=i+1
	end

	local i = 1
	while true do
		local name, value = debug.getupvalue(f1, i)
		if name == nil then
			break
		end
		print("patch:",name,f1,i,f2,uv[name])
		if uv[name] then
			debug.upvaluejoin(f1,i,f2,uv[name])
		end
		i=i+1
	end
end

local profile_table = {}

local function update_stat(name, ti)
	local t = profile_table[name]
	if t == nil then
		t = { count = 0,  time = 0 }
		profile_table[name] = t
	end
	t.count = t.count + 1
	t.time = t.time + ti
end

local traceback = debug.traceback

local function return_f(f, ...)
	return skynet.ret(skynet.pack(f(...)))
end

local function timing( method, ... )
	local err, msg
	profile.start()
	if method[2] == "accept" then
		-- no return
		err,msg = xpcall(method[4], traceback, ...)
	else
		err,msg = xpcall(return_f, traceback, method[4], ...)
	end
	local ti = profile.stop()
	update_stat(method[3], ti)
	assert(err,msg)
end

skynet.start(function()
	local init = false
	local dispatcher
	local router
	local substart
	local sub_init
	local max_system_id
	for id,method in ipairs(func) do
		if method[2]=="system" then 
			if not max_system_id or id>max_system_id then
				max_system_id=id
			end
			if method[3]=="dispatch" then
				dispatcher=method[4]
			elseif method[3] == "route" then
				router=method[4]
			elseif method[3] == "substart" then
				substart=method[4]
			elseif method[3] == "init" then
				sub_init=method[4]
			end
		end
	end

	_ENV.dft_dispatcher=function( session , source , id, ...)
		local method = func[id] or error("func id error:"..tostring(id))
		if method[2] == "system" then
			local command = method[3]
			if command == "hotfix" then
				local hotfix = require "smg.hotfix"
				skynet.ret(skynet.pack(hotfix(func, ...)))
			elseif command == "subhotfix" then
				for k,v in ipairs(sublist) do
					smg.hotfix(v,...)
				end
			elseif command == "sublist" then
				local list={}
				for k,v in ipairs(sublist) do
					table.insert(list,v.handle)
				end
				skynet.ret(skynet.pack(subsrv_name,list))
			elseif command == "profile" then
				skynet.ret(skynet.pack(profile_table))
			elseif command == "init" then
				assert(not init, "Already init")
				local initfunc = method[4] or function() end
				initfunc(...)
				if substart then
					subid=0
					substart()
				end
				skynet.ret()
				skynet.info_func(function()
					return profile_table
				end)
				init = true
			elseif command == "substart" then
				substart=nil
				subid=...
				assert(not init, "Already init")
				sub_init(select(1,...))
				skynet.ret()
				skynet.info_func(function()
					return profile_table
				end)
				init = true
			else
				assert(init, "Never init")
				assert(command == "exit")
				local exitfunc = method[4] or function() end
				if sublist then
					for k,v in pairs(sublist) do
						local r,err=pcall(smg.kill,v,...)
						if not r then
							error("sub srv exit error:"..tostring(k))
						end
						sublist[k]=nil
					end
				end
				exitfunc(...)
				skynet.ret()
				init = false
				skynet.exit()
			end
		else
			assert(init, "Init first")
			timing(method, ...)
		end
	end

	local hotfix_dispatch=function( session , source ,group,id, ...)
		local method = hotfix_func[group][id]
		timing(method, ...)
	end

	skynet.dispatch("smg", dispatcher or dft_dispatcher)
	skynet.dispatch("smg_hotfix",hotfix_dispatch)

	-- set lua dispatcher
	function smg.enablecluster()
		enablecluster=true
		skynet.dispatch("lua", dispatcher or dft_dispatcher)
	end

	function smg.start_subsrv(subsrv_name,num,...)
		_ENV.sublist={}
		_ENV.subsrv_name=subsrv_name
		local sub1
		for i=1,num do
			local handle=smg.substart(subsrv_name,i,...)
			table.insert(sublist,handle)
		end
		sub1=sublist[1]


		local balance
		if not router then
			balance=1
			router=function()
				balance = balance + 1
				if balance > num then
					balance = 1
				end	
				return balance
			end
		end
		if subsrv_name ~= SERVICE_NAME then
		--当子服务和主服务不是同一个文件时不能存在同主服务同名的接口	
			for id,method in ipairs(func) do
				if method[2]=="accept" or method[2] == "response" then 
					assert(not sub1.func.accept[method[3]] and not sub1.func.response[method[3]],method[3])
				end
			end
			skynet.dispatch("smg", function(session , source ,method,id,...)
				if type(method) == "number" then
					return (dispatcher or dft_dispatcher)(session,source,method,id,...)
				end
				balance=router(...)
				if not balance then--不需要子服务处理
					return (dispatcher or dft_dispatcher)(session,source,method,id,...)
				end
				if balance >num then
					balance=balance%num+1
				end
				local handle=sublist[balance]
				id=handle.func[method][id] or error("func id error:"..tostring(method..","..tostring(id)))
				skynet.redirect(handle.handle, source,"smg",session,skynet.pack(id,...))
				if skynet.ignoreret then
					skynet.ignoreret()
				end
			end)
		else
			skynet.dispatch("smg", function(session , source ,method,id,...)
				if type(method) == "number" and method<=max_system_id then
					return (dispatcher or dft_dispatcher)(session,source,method,id,...)
				end
				balance=router(...)
				if not balance then--不需要子服务处理
					return (dispatcher or dft_dispatcher)(session,source,method,id,...)
				end
				if balance >num then
					balance=balance%num+1
				end
				local handle=sublist[balance]
				skynet.redirect(handle.handle, source,"smg",session,skynet.pack(method,id,...))
				if skynet.ignoreret then
					skynet.ignoreret()
				end
			end)
		end
	end
end)
