local M={}
local data=100
local function closure()
	local i=1
	return function(p)
		print("test_smg_hotfix",i,data,p)
		i=i+1
	end
end
M.print=closure()
M.print2=closure()

return M
