local skynet = require "skynet"
local snax = require "skynet.smg"

skynet.start(function()
	--skynet.trace()
	local ps = snax.newservice ("pingserver_smg", "hello world")
	print(ps.req.ping("foobar"))
	print(ps.post.hello())
	--print(pcall(ps.req.error))
	--skynet.exit()
	print("Hotfix (i) :", snax.hotfix(ps, [[

local i
local hello

function accept.hello()
	i = i + 1
	print ("fix", i, hello)
end

function dispatch(session , source , id,...)
	print("=======================================dispatch2:",func[id][3],id,...)
	dft_dispatcher(session,source,id,...)
end

function exit(...)
	print ("ping server exit2:", ...)
end

function hotfix(...)
	local temp = i
	i = 100
	print("hotfix call hello2")
	accept.hello2()
	print("hotfix:",i,hello,accept)
	accept.hello3=function(p)
		print("accept.hello3",hello,p)
	end
	response.hello3=function(p)
		return p
	end
	return temp
end

	]]))
	print(ps.post.hello())
	print(ps.hpost.hello3("HHHHHHHHHHHHHH hello3"))
	print("................",ps.hreq.hello3("HHHHHHHHHHHHHH hello3"))
	skynet.exit()

	local info = skynet.call(ps.handle, "debug", "INFO")

	for name,v in pairs(info) do
		print(string.format("%s\tcount:%d time:%f", name, v.count, v.time))
	end

	print(ps.post.exit("exit")) -- == snax.kill(ps, "exit")
	skynet.exit()
end)
