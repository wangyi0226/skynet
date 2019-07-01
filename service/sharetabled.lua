local skynet = require "skynet"
local core = require "skynet.sharetable.core"
local mc = require "skynet.multicast"
local matrix = {}	-- all the matrix
local files = {}	-- filename : matrix
local clients = {}
local sharetable = {}
local channel
local function close_matrix(m)
	if m == nil then
		return
	end
	local ptr = m:getptr()
	local ref = matrix[ptr]
	if ref == nil or ref.count == 0 then
		matrix[ptr] = nil
		m:close()
	end
end

function sharetable.loadfile(source, filename, ...)
	close_matrix(files[filename])
	local m = core.matrix("@" .. filename, ...)
	files[filename] = m
	skynet.fork(channel.publish,channel,filename)
	skynet.ret()
end

function sharetable.loadstring(source, filename, datasource, ...)
	close_matrix(files[filename])
	local m = core.matrix(datasource, ...)
	files[filename] = m
	skynet.fork(channel.publish,channel,filename)
	skynet.ret()
end

local function loadtable(filename, ptr, len)
	close_matrix(files[filename])
	local m = core.matrix([[
		local unpack, ptr, len = ...
		return unpack(ptr, len)
	]], skynet.unpack, ptr, len)
	files[filename] = m
end

function sharetable.loadtable(source, filename, ptr, len)
	local ok, err = pcall(loadtable, filename, ptr, len)
	skynet.trash(ptr, len)
	assert(ok, err)
	skynet.fork(channel.publish,channel,filename)
	skynet.ret()
end

local function query_file(source, filename)
	local m = files[filename]
	local ptr = m:getptr()
	local ref = matrix[ptr]
	if ref == nil then
		ref = {
			filename = filename,
			count = 0,
			matrix = m,
			refs = {},
		}
		matrix[ptr] = ref
	end
	if ref.refs[source] == nil then
		ref.refs[source] = true
		local list = clients[source]
		if not list then
			clients[source] = { ptr }
		else
			table.insert(list, ptr)
		end
		ref.count = ref.count + 1
	end
	return ptr
end

function sharetable.query(source, filename)
	local m = files[filename]
	if m == nil then
		skynet.ret()
		return
	end
	local ptr = query_file(source, filename)
	skynet.ret(skynet.pack(ptr))
end

function sharetable.close(source)
	local list = clients[source]
	if list then
		for _, ptr in ipairs(list) do
			local ref = matrix[ptr]
			if ref and ref.refs[source] then
				ref.refs[source] = nil
				ref.count = ref.count - 1
				if ref.count == 0 then
					if files[ref.filename] ~= ref.matrix then
						-- It's a history version
						skynet.error(string.format("Delete a version (%s) of %s", ptr, ref.filename))
						ref.matrix:close()
						matrix[ptr] = nil
					end
				end
			end
		end
		clients[source] = nil
	end
	-- no return
end
function sharetable.channel()
	skynet.ret(skynet.pack(channel.channel))
end

skynet.start(function()
	channel=mc.new()
	skynet.dispatch("lua", function (session, source ,cmd, ...)
		sharetable[cmd](source,...)
	end)
end)
