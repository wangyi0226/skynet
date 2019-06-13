local skynet = require "skynet"
local smg = require "skynet.smg"

function response.ping_s(...)
	skynet.error("==============subping_s:",subid,...)
	return subid
end

function init(...)
	print ("sub server start:",subid,...)
end

function exit(...)
	print ("sub server exit:",subid,...)
end
