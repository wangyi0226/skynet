local skynet = require "skynet"
local smg = require "skynet.smg"

function response.ping(...)
	skynet.error("==============subping:",subid,...)
	return subid
end

--[[
function route(...)
	return math.random(100)
end
]]

function substart()
	smg.start_subsrv(SERVICE_NAME,5)
end

function init()
	print ("sub server start:",subid)
end

function exit(...)
	print ("sub server exit:",subid)
end
