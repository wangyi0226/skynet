local skynet = require "skynet"
local hashsi = require "hashsi"
local mode = ...

local MAX=60
local HASHSI_TABLE={
	TEST={id=1,max=MAX},
	AGENT={id=2,max=MAX},
	TEST2={id=3,max=MAX},
}

if tonumber(mode) then

local function test_insert(si,si2)
	for i=1,MAX do
		si[i]=i*100+tonumber(mode)
		si2[i]=i*100+tonumber(mode)
		skynet.sleep(math.random(3))
	end
	for k,v in pairs(si) do
		skynet.sleep(math.random(3))
	end
	skynet.exit()
end

local function test_remove(si,si2)
	for i=1,MAX do
		si[i]=nil
		si2[i]=nil
		skynet.sleep(math.random(3))
	end
	for k,v in pairs(si) do
		skynet.sleep(math.random(3))
	end
	skynet.exit()
end

skynet.start(function()
	local si=hashsi.table(HASHSI_TABLE.TEST.id)
	local si2=hashsi.table("test")
	if math.random(100)<50 then
		skynet.fork(test_remove,si,si2)
		--skynet.fork(test_insert,si,si2)
	else
		skynet.fork(test_insert,si,si2)
	end
	skynet.dispatch("lua", function (...)
		print("dispatch:",...)
	end)
end)
else
	local si
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
		hashsi.new({name="test",max=100})
		si=hashsi.table(HASHSI_TABLE.TEST2.id)
		for c=1,10000000 do
			update_value(math.random(MAX))
		end
		print("=============================")
		for i=1,100 do
			skynet.newservice(SERVICE_NAME,i)	-- launch self in test mode
		end
		skynet.sleep(500)
		local si2=hashsi.table("test")
		local si3=hashsi.table(HASHSI_TABLE.TEST.id)
		local count=0
		for k,v in pairs(si3) do
			count=count+1
			print("==========",k,v)
		end
		print(count,hashsi.count(si3))
		count=0
		for k,v in pairs(si2) do
			count=count+1
			print("==========",k,v)
		end
		print(count,hashsi.count(si2))
	end)
end
