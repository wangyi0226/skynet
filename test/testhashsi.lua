local skynet = require "skynet"
local hashsi = require "hashsi"
local mode = ...
local HASHSI_TABLE={
	TEST=1
}

local MAX=60

if tonumber(mode) then

local function test_insert(si)
	for i=1,MAX do
		si[i]=i*100+tonumber(mode)
	end
	skynet.exit()
end

local function test_remove(si)
	for i=1,MAX do
		si[i]=nil
	end
	skynet.exit()
end

skynet.start(function()
	local si=hashsi.table(HASHSI_TABLE.TEST)
	if math.random(100)<50 then
		skynet.fork(test_remove,si)
	else
		skynet.fork(test_insert,si)
	end
end)
else
	skynet.start(function()
		hashsi.init({MAX})
		for i=1,100000 do
			skynet.newservice(SERVICE_NAME,i)	-- launch self in test mode
		end
		local si=hashsi.table(HASHSI_TABLE.TEST)
		for i=1,100 do
			--print(i,si[i])
		end
	end)
end
