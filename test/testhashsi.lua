local skynet = require "skynet"
local hashsi = require "hashsi"
local mode = ...

local MAX=60
local HASHSI_TABLE={
	TEST={id=1,max=MAX},
	AGENT={id=2,max=MAX},
}

if tonumber(mode) then

local function test_insert(si)
	for i=1,MAX do
		si[i]=i*100+tonumber(mode)
		skynet.sleep(math.random(3))
	end
	skynet.exit()
end

local function test_remove(si)
	for i=1,MAX do
		si[i+MAX]=nil
		skynet.sleep(math.random(3))
	end
	skynet.exit()
end

skynet.start(function()
	local si=hashsi.table(HASHSI_TABLE.TEST.id)
	--if math.random(100)<50 then
		skynet.fork(test_remove,si)
	else
		skynet.fork(test_insert,si)
	end
	skynet.dispatch("lua", function (...)
		print("dispatch:",...)
	end)
end)
else
	skynet.start(function()
		hashsi.init(HASHSI_TABLE)
		local si=hashsi.table(HASHSI_TABLE.TEST.id)
		for i=1,100 do
			skynet.newservice(SERVICE_NAME,i)	-- launch self in test mode
		end
		skynet.sleep(500)
		local si=hashsi.table(HASHSI_TABLE.TEST.id)
		local count=0
		for k,v in pairs(si) do
			count=count+1
			print("==========",k,v)
		end
		print(count,hashsi.count(si))
	end)
end
