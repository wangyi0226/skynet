local skynet = require "skynet"
local smg = require "skynet.smg"

skynet.start(function()
	--local ps = smg.uniqueservice("subserver_smg")
	local ps = smg.newservice("subserver_smg")
	for i=1,10 do
		print(ps,ps.req)
		print("response",ps.req.ping("AAA","BBB","CCC"))
	end
	--hotfix subsrv
	print("Hotfix (i) :", smg.subhotfix(ps, [[
		function response.ping(...)
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
		print(k,v,v.req.ping("========="))
	end
	smg.kill(ps)
	skynet.exit()
end)
