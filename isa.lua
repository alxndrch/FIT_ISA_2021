-- author: Alexandr Chalupnik
-- email: xchalu15@stud.fit.vutbr.cz

SKIP_PARENTHESE = 1
SKIP_SPACE = 1
SKIP_QUOTE_MARK = 1

local isa_proto = Proto("ISA", "ISA PROTOCOL")

local lenght = ProtoField.uint64("isa.lenght", "Lenght", base.DEC)
local status = ProtoField.string("isa.status", "Status")
local msg_type = ProtoField.string("isa.type", "Type")

isa_proto.fields = { lenght, status, msg_type }

function isa_proto.dissector(buff, pinfo, tree)

    local read = dissectISA(buff, pinfo, tree)

    if read == 0 then
        return 0
    end

    return read
end

dissectISA = function(buff, pinfo, tree)

    if buff(0,1):string() ~= "(" then
        return 0
    end

    local pktlen = buff:len()
    local offset = 0
    local lenght
    local status

    local subtree = tree:add(isa_proto, buff(), "ISA Protocol Data")

    if pinfo.src_port == 50000 then

        status, offset = evalStatus(buff, pinfo, subtree)
        
        if status == "ok" then
            if buff(1,4):string() == "list" then
                command = "list"
	        subtree:add(buff(1,4),"Command: " .. buff(1,4):string())
            elseif buff(1,4):string() == "send" then
                command = "send"
	        subtree:add(buff(1,4),"Command: " .. buff(1,4):string())
	    elseif buff(1,5):string() == "fetch" then
                command = "fetch"
	        subtree:add(buff(1,5),"Command: " .. buff(1,5):string())
            elseif buff(1,5):string() == "login" then
                command = "login"
	        subtree:add(buff(1,5),"Command: " .. buff(1,5):string())
            elseif buff(1,6):string() == "logout" then
                command = "logout"
	        subtree:add(buff(1,6),"Command: " .. buff(1,6):string())
            elseif buff(1,8):string() == "register" then
                command = "register"
	        subtree:add(buff(1,8),"Command: " .. buff(1,8):string())
            else 
                return 0
            end

        else
            offset = offset + SKIP_QUOTE_MARK
            lenght = pktlen - offset - SKIP_PARENTHESE - SKIP_QUOTE_MARK
            subtree:add(buff(offset, lenght),"Type: " .. buff(offset, lenght):string())
        end

    else 

       local command

        if buff(1,4):string() == "list" then
            command = "list"
	    subtree:add(buff(1,4),"Command: " .. buff(1,4):string())
        elseif buff(1,4):string() == "send" then
            command = "send"
	    subtree:add(buff(1,4),"Command: " .. buff(1,4):string())
	elseif buff(1,5):string() == "fetch" then
            command = "fetch"
	    subtree:add(buff(1,5),"Command: " .. buff(1,5):string())
        elseif buff(1,5):string() == "login" then
            command = "login"
	    subtree:add(buff(1,5),"Command: " .. buff(1,5):string())
        elseif buff(1,6):string() == "logout" then
            command = "logout"
	    subtree:add(buff(1,6),"Command: " .. buff(1,6):string())
        elseif buff(1,8):string() == "register" then
            command = "register"
	    subtree:add(buff(1,8),"Command: " .. buff(1,8):string())
        else 
            return 0
        end

        pinfo.cols.info = "Request: " .. command

    end




    pinfo.cols.protocol = isa_proto.name

    return 1000
end

evalStatus = function(buff, pinfo, tree)

    if buff(1,2):string() == "ok" then
        status = "ok"
        lenght = 2
    elseif buff(1,3):string() == "err" then
        status = "err"
        lenght = 3
    else
        return 0
    end

    tree:add(buff(1,lenght), "Status: " .. buff(1, lenght):string())
    pinfo.cols.info = "Response: " .. status

    return status, 1 + lenght + SKIP_SPACE

end


local tcp_port = DissectorTable.get("tcp.port")
tcp_port:add(50000, isa_proto)