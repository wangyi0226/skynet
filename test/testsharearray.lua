local skynet = require "skynet"
local sharearray = require "sharearray.core"
local hashsi = require "hashsi"
local shop_num=1
local shopitem_total_num=30
local shopitem_update_num=10
local shopitem_num=5
local usednum=shopitem_total_num-shopitem_update_num
local randnum=usednum//shopitem_num

local SA_TINT=1
local SA_TSTRING=2
local SA_TPOINTER=3
local function sa2str(sa)
	local info=sharearray.info(sa)
	return string.format("ri:%d,ui:%d,size:%d\n",info.ri,info.ui,info.size)
end

local mode = ...
if tonumber(mode) then
local severid=tonumber(mode)
local shopid=severid%shop_num+1

skynet.start(function()
	local shop=hashsi.table("randomshop")[shopid]
	for i=1,10 do
		local r=math.random(randnum)
		local index=sharearray.index(shop,r)
		print(severid,shopid,r,index,sa2str(shop))
		local itemlist=sharearray.range(shop,index,index+shopitem_num)
		print("list",table.unpack(itemlist))
		print("has:",sharearray.has(shop,1221,index,index+shopitem_num),sharearray.has(shop,itemlist[1],index,index+shopitem_num))
		skynet.sleep(100)
	end
end)

else
skynet.start(function()
	local list={}
	local randomshop=hashsi.new("randomshop")
	for shopid=1,shop_num do
		print("============ init shop",shopid)
		randomshop[shopid]=sharearray.new(shopitem_total_num,SA_TINT)
		local list={}
		for j=1,shopitem_total_num do
			local shopitemid=shopid*10000000+j
			table.insert(list,shopitemid)
		end
		local ri,ui=0,usednum
		sharearray.init(randomshop[shopid],list,ri,ui)
		print("info:",sa2str(randomshop[shopid]))
	end	
	for i=1,shop_num do
		skynet.fork(skynet.newservice,SERVICE_NAME,i)	-- launch self in test mode
	end
	for update=1,5 do
		for shopid=1,shop_num do
			local list={}
			for j=1,shopitem_update_num do
				local shopitemid=shopid*10000000+update*10+j
				table.insert(list,shopitemid)
			end
			sharearray.update(randomshop[shopid],list)
			print("update:",sa2str(randomshop[shopid]))
		end
		skynet.sleep(100)
	end
end)

end
