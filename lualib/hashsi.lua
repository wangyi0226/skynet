local core=require"hashsi.core"
local M={}
local meta={}

meta.__index=function(self,k)
    return core.get(rawget(self,"__id"),k)
end

meta.__newindex=function(self,k,v)
    return core.set(rawget(self,"__id"),k,v)
end

function M.init(list)
    core.init(list)
end

function M.table(id)
    return setmetatable({__id=id},meta)
end

return M