local skynet = require "skynet"
local smg = require "skynet.smg"

skynet.start(function()
	local ps = smg.uniqueservice("subserver_smg")
	local ps2 = smg.uniqueservice("subserver_smg")
	print(ps,ps2,ps==ps2)
	for i=1,10 do
		--subagent没有ping方法时调用主服务的
		--subagent不能存在同主服务同名的接口
		print("response",ps.req.ping("AAA","BBB","CCC"))
	end
	for i=1,10 do
		--print("response",ps.req.ping_s("AAA","BBB","CCC"))
	end
	--skynet.exit()

	--hotfix subsrv
	print("Hotfix (i) :", smg.hotfix(ps, [[
		function response.ping(...)
			print("============================hotfix ping:",subid,...)
			return subid
		end
	]]))		
	--hotfix subsrv
	print("Hotfix (i) :", smg.subhotfix(ps, [[
		function response.ping_s(...)
			print("============================hotfix subping:",subid,...)
			return subid
		end
	]]))
	for i=1,10 do
		print("response",ps.req.ping("AAA","BBB","CCC"))
	end
	local sublist=smg.sublist(ps)
	print("=================== start sublist")
	for k,v in pairs(sublist) do
		print(k,v,v.req.ping_s("========="))
	end
	smg.kill(ps)
	skynet.exit("exitcall")
end)
