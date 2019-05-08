local skynet = require "skynet"
local c = require "skynet.core"
local smg_interface = require "smg.interface"
local profile = require "skynet.profile"
local smg = require "skynet.smg"

local smg_name = tostring(...)
local loaderpath = skynet.getenv"smg_loader"
local loader = loaderpath and assert(dofile(loaderpath))
local func, pattern,smg_env = smg_interface(smg_name, _ENV, loader)
local smg_path = pattern:sub(1,pattern:find("?", 1, true)-1) .. smg_name ..  "/"
package.path = smg_path .. "?.lua;" .. package.path

SERVICE_NAME = smg_name
SERVICE_PATH = smg_path

_ENV.accept=smg_env.accept
_ENV.response=smg_env.response
_ENV.dft_dispatcher=nil
_ENV.enablecluster=nil
_ENV.func=func

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
	for id,method in pairs(func) do
		if method[2]=="system" and method[3]=="dispatch" then
			dispatcher=method[4]
		end
	end
	_ENV.dft_dispatcher=function( session , source , id, ...)
		local method = func[id]

		if method[2] == "system" then
			local command = method[3]
			if command == "hotfix" then
				local hotfix = require "smg.hotfix"
				skynet.ret(skynet.pack(hotfix(func, ...)))
			elseif command == "profile" then
				skynet.ret(skynet.pack(profile_table))
			elseif command == "init" then
				assert(not init, "Already init")
				local initfunc = method[4] or function() end
				initfunc(...)
				skynet.ret()
				skynet.info_func(function()
					return profile_table
				end)
				init = true
			else
				assert(init, "Never init")
				assert(command == "exit")
				local exitfunc = method[4] or function() end
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
	skynet.dispatch("smg", dispatcher or dft_dispatcher)

	-- set lua dispatcher
	function smg.enablecluster()
		enablecluster=true
		skynet.dispatch("lua", dispatcher or dft_dispatcher)
	end
end)
