local skynet = require "skynet"
local hashsi = require "hashsi"
local mode = ...

local MAX=60
local snum=100
local HASHSI_TABLE={
	TEST={id=1,max=MAX},
	AGENT={id=2,max=snum},
	TEST2={id=3,max=MAX},
	TEST3={id=4},
}
local MAX_HASHCAP=nil
if tonumber(mode) then

local function test_insert(si)
	for i=1,MAX do
	    local n=math.random(3)
		if n == 1 then
			si[i]=i*100+tonumber(mode)
		elseif n == 2 then
			si[i]=(i*100)..mode
		else
			si[i]=skynet.pack({a=i})
		end
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

	local si2=hashsi.table("test")
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
		local n=math.random(3)
		if n == 1  then
			data[i]=tostring(data[i])
		elseif n==2 then
			data[i]=skynet.pack(data[i])
		end
		if data[i]==0 then
			si[i]=nil
			data[i]=nil
		else
			si[i]=data[i]
		end
		if data[i]~=si[i] and false then
			print(i,data[i],si[i],type(data[i]),type(si[i]))
			error("============================")
		end
	end
	skynet.start(function()
	    for i=1,1000 do
	        local si=hashsi.new("testsi"..i)
	        si["id"]=i
	    end
	    for i=1,1000 do
	        local si=hashsi.table("testsi"..i)
	        assert(i==si["id"])
	    end
		hashsi.init(HASHSI_TABLE,MAX_HASHCAP)
		si=hashsi.table(HASHSI_TABLE.TEST2.id)
		si2=hashsi.new("test")
		print("====================1")
		for c=1,1000 do
			update_value(math.random(MAX))
		end
		print("====================2")
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
		local si4=hashsi.table(HASHSI_TABLE.TEST3.id)
		local count=0
		local size=1000000
		for i=0,size-1 do
			si4[i]=i
		end
		print(hashsi.count(si4),size)
		assert(hashsi.count(si4)==size)
		for i=0,size-1 do
			si4[i]=nil
		end
		print(hashsi.count(si4))
		assert(hashsi.count(si4)==0)
		local m={}
		for i=0,size-1 do
			local index=math.random(100000)
			if math.random(2)==1 then
				si4[index]=nil
				m[index]=nil
			else
				si4[index]=i
				m[index]=i
			end
		end
		for k,v in pairs(m) do
			count=count+1
			assert(si4[k]==v)
		end
		local count2=0
		local flag={}
		for k,v in pairs(si4) do
			count2=count2+1
			assert(m[tonumber(k)]==v)
			assert(flag[k]==nil)
			flag[k]=v
			m[tonumber(k)]=nil
		end
		for k,v in pairs(m) do
			error("remain",k,si4[k],v)
		end
		assert(hashsi.count(si4)==count)
	end)
end
