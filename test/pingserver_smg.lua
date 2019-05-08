local skynet = require "skynet"
local queue = require "skynet.queue"
local snax = require "skynet.smg"

local i = 0
local hello = "hello"

function response.ping(hello)
	skynet.sleep(100)
	return hello
end

-- response.sleep and accept.hello share one lock
local lock

function accept.sleep(queue, n)
	if queue then
		lock(
		function()
			print("queue=",queue, n)
			skynet.sleep(n)
		end)
	else
		print("queue=",queue, n)
		skynet.sleep(n)
	end
end

function accept.hello()
	accept.hello2()
	lock(function()
	i = i + 1
	print (i)
	--print (i, hello)
	end)
end

function accept.hello2()
	print("========================:hello2")
end

function accept.exit(...)
	snax.exit(...)
end

function response.error()
	error "throw an error"
end

function dispatch(...)
	print("=======================================dispatch:")
	dft_dispatcher(...)
end

function init( ... )
	print ("ping server start:", ...)
	--snax.enablecluster()	-- enable cluster call
	-- init queue
	lock = queue()
end

function exit(...)
	print ("ping server exit:", ...)
end
