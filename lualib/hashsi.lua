local core=require"hashsi.core"
local M={}
local meta={}

local function next_closure(self)
    local index=0
    local id=rawget(self,"__id")
    return function()
        local key,val
        index,key,val=core.next(id,index)
		return key,val
	end
end

meta.__index=function(self,k)
    return core.get(rawget(self,"__id"),k)
end

meta.__newindex=function(self,k,v)
    --if not v then return end
    return core.set(rawget(self,"__id"),k,v)
end

meta.__pairs=function(self)
    return next_closure(self),self,nil
end

function M.count(t)
    return core.count(rawget(t,"__id"))
end
function M.init(conf)
    local list={}
    local size=0
    for k,v in pairs(conf) do
        if v.id>size then
            size=v.id
        end
        assert(list[v.id]==nil,"id exist:"..v.id)
        list[v.id]=v.max
    end
    assert(size==#list)
    core.init(list)
end

function M.table(id)
    assert(type(id)=="number")
    return setmetatable({__id=id},meta)
end

return M