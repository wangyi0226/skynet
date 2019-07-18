local skynet = require "skynet"
local snax = require "skynet.smg"

skynet.start(function()
	--skynet.trace()
	local ps = snax.newservice ("pingserver_smg", "hello world")
	print(ps.req.ping("foobar"))
	print("AAAAAAAAAAAAAAAAAAAAAAAAAAA:",ps.wait.ping2("foobar"))
	print(ps.post.hello())
	--print(pcall(ps.req.error))
	--skynet.exit()
	print("Hotfix (i) :", snax.hotfix(ps, [[

local i
local hello2_old_uv
local test_smg_hotfix
local lock
local skynet

function wait.ping2(hello)
	local r=skynet.response()
	r(true,hello.."#2")
end

function accept.hello()
	i = i + 1
	print ("fix", i, hello)
end

function accept.hello2(...)
	print("========================== hello2_old_uv",hello2_old_uv,...)
	if not hello2_old_uv then
		local i,data
		local new_p=function(p)
			if data == 100 then
				data=1000
			end
			print("test_smg_hotfix----",i,data,p)
			i=i+1
		end
		__patch(new_p,test_smg_hotfix.print)
		test_smg_hotfix.print=new_p
	end
	test_smg_hotfix.print(...)	
end

function dispatch(method, ... )
	print("=======================================dispatch2:",method[1],method[2],method[3],method[4],...)
	method[4](...)
end

function exit(...)
	print ("ping server exit2:", ...)
end

function hotfix(...)
	local temp = i
	i = 100
	print("hotfix call hello2",hotfix_val)
	accept.hello2()
	print("hotfix:",i,hello,accept)
	accept.hello3=function(p)
		print("accept.hello3",hello,p)
		test_smg_hotfix.print2(p)
	end
	response.hello3=function(p)
		return p
	end

	wait.ping3=function(p)
		local r=skynet.response()
		r(true,p.."#")
	end
	return temp
end

	]]))
	print(ps.post.hello())
	print(ps.post.hello2("HHHHHHHHHHHHHHHHHHHHHHH1"))
	print("wait==========",ps.wait.ping2("HHHHHHHHHHHHHHHHHHHHHHH2"))
	print(ps.hpost.hello3("HHHHHHHHHHHHHHpost hello3"))
	print("PPPPPP",ps.hwait.ping3("HHHHHHHHHHHHHHwait ping3"))
	print("................",ps.hreq.hello3("HHHHHHHHHHHHHHreq hello3"))
	skynet.exit()

	local info = skynet.call(ps.handle, "debug", "INFO")

	for name,v in pairs(info) do
		print(string.format("%s\tcount:%d time:%f", name, v.count, v.time))
	end

	print(ps.post.exit("exit")) -- == snax.kill(ps, "exit")
	skynet.exit()
end)
