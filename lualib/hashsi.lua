local core=require"hashsi.core"
local M={}
local meta={}

local function next_closure(self)
    local i,j=0,-1
    local id=rawget(self,"__id")
    return function()
        local key,val
        i,j,key,val=core.next(id,i,j)
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

function M.init(conf,max_hashcap)
    local list={}
    local size=0
    for k,v in pairs(conf) do
        if v.id>size then
            size=v.id
        end
        assert(list[v.id]==nil,"id exist:"..v.id)
        list[v.id]=v.max or 0
    end
    assert(size==#list)
    core.init(list,max_hashcap or 65536)
end

function M.table(id)
    assert(type(id)=="number" or type(id)=="string")
    return setmetatable({__id=id},meta)
end

return M