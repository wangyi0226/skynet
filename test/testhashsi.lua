local skynet = require "skynet"
local hashsi = require "hashsi"
local mode = ...

local MAX=60
local snum=10000
local HASHSI_TABLE={
	TEST={id=1,max=MAX},
	AGENT={id=2,max=snum},
	TEST2={id=3,max=MAX},
}

if tonumber(mode) then

local function test_insert(si)
	for i=1,MAX do
		si[i]=i*100+tonumber(mode)
		--skynet.sleep(math.random(3))
	end
	for k,v in pairs(si) do
		--skynet.sleep(math.random(3))
	end
end

local function test_remove(si)
	for i=1,MAX do
		si[i]=nil
		--skynet.sleep(math.random(3))
	end
	for k,v in pairs(si) do
		--skynet.sleep(math.random(3))
	end
end

skynet.start(function()
	local si=hashsi.table(HASHSI_TABLE.TEST.id)
	if math.random(100)<50 then
		skynet.fork(test_remove,si)
		--skynet.fork(test_insert,si)
	else
		skynet.fork(test_insert,si)
	end

	local si2=hashsi.table(HASHSI_TABLE.AGENT.id)
	si2[mode]=1
	skynet.exit()
	skynet.dispatch("lua", function (...)
		print("dispatch:",...)
	end)
end)
else
	local si
	local si2
	local data={}
	local function update_value(i)
		data[i]=math.random(100)-1
		if data[i]==0 then
			si[i]=nil
			data[i]=nil
		else
			si[i]=data[i]
		end
		if data[i]~=si[i] then
			print(i,data[i],si[i])
			error("============================")
		end
	end
	skynet.start(function()
		hashsi.init(HASHSI_TABLE)
		si=hashsi.table(HASHSI_TABLE.TEST2.id)
		si2=hashsi.table(HASHSI_TABLE.AGENT.id)
		for c=1,100000 do
			update_value(math.random(MAX))
		end
		for i=1,snum do
			skynet.error("=============================",i)
			skynet.newservice(SERVICE_NAME,i)	-- launch self in test mode
		end
		while hashsi.count(si2) ~= snum  do
			skynet.error("sleep",hashsi.count(si2),snum)
			skynet.sleep(100)
		end
		local si3=hashsi.table(HASHSI_TABLE.TEST.id)
		local count=0
		for k,v in pairs(si3) do
			count=count+1
			skynet.error("==========",k,v)
		end
		assert(count == hashsi.count(si3))
	end)
end
