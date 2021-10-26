-- author: Alexandr Chalupnik
-- email: xchalu15@stud.fit.vutbr.cz

SKIP_PARENTHESE = 1
SKIP_SPACE = 1
SKIP_QUOTE_MARK = 1

LEN_REGISTERED = 15
LEN_LOGGED_IN = 14
LEN_MSG_SEND = 12
LEN_LOGGED_OUT = 10

LEN_LIST = 4
LEN_SEND = 4
LEN_FETCH = 5
LEN_LOGIN = 5
LEN_LOGOUT = 6
LEN_REGISTER = 8

STATE_MSG_BLOCK = 0
STATE_NONE = 0
STATE_NUM = 1
STATE_USER = 2
STATE_SUB = 3
STATE_BODY = 4
STATE_PWD = 5

STATE_READ = -1
STATE_STOP = -2

RET_SUCC = 1
RET_ERR = 0

local isa_proto = Proto("ISA", "ISA PROTOCOL")

function isa_proto.dissector(buff, pinfo, tree)

    local read = dissectISA(buff, pinfo, tree)

    if read == 0 then
        return RET_ERR
    end

    return read
end

dissectISA = function(buff, pinfo, tree)

    if buff(0,1):string() ~= "(" then
        return RET_ERR
    end

    local pktlen = buff:len()
    local offset = 0
    local lenght
    local status

    local subtree = tree:add(isa_proto, buff(), "ISA Protocol Data")
    local temp_tree

    if pinfo.src_port == 50000 then

        status, offset = evalStatus(buff, pinfo, subtree)
        local msg_tree
        
        if status == "ok" then
            offset = offset + SKIP_PARENTHESE

            if buff(offset-SKIP_PARENTHESE, 1):string() == "(" then
                
                local temp_offset = offset
                local STATE = STATE_MSG_BLOCK
                local begin = 0 
                local lenght = 0

                temp_offset = offset - SKIP_PARENTHESE
                if buff(offset, 1):string() == "\"" then
                    temp_offset = offset - SKIP_QUOTE_MARK - SKIP_PARENTHESE
                    temp_tree = subtree:add(buff(offset, lenght),"Type: fetch")

                    while (temp_offset ~= pktlen-1) do 
                        temp_offset = temp_offset + 1
                        if buff(temp_offset, 1):string() == "(" and STATE == STATE_MSG_BLOCK then
                            STATE = STATE_USER
                        elseif STATE == STATE_USER then
                            begin, lenght, STATE = textEval(buff, temp_offset, begin, lenght, " ",
                                                            STATE_USER, STATE_SUB, "To: ", temp_tree)
                        elseif STATE == STATE_SUB then
                            begin, lenght, STATE = textEval(buff, temp_offset, begin, lenght, " ",
                                                            STATE_SUB, STATE_BODY, "Subject: ", temp_tree)
                        elseif STATE == STATE_BODY then
                            begin, lenght, STATE = textEval(buff, temp_offset, begin, lenght, ")",
                                                            STATE_BODY, STATE_MSG_BLOCK, "Text: ", temp_tree)
                        end
                    end 
                    
                    return RET_SUCC
                end 

                temp_tree = subtree:add(buff(offset, lenght),"Type: messages")    
                if buff(offset+SKIP_PARENTHESE, 1):string() == ")" then
                    temp_tree:add(buff(),"Number of messages: 0")
                else
                    while (temp_offset ~= pktlen-1) do 
                        temp_offset = temp_offset + 1
                        
                        if buff(temp_offset, 1):string() == "(" and STATE == STATE_MSG_BLOCK then
                            STATE = STATE_NUM
                        elseif STATE == STATE_NUM then
                            if begin == 0 then
                                begin = temp_offset
                                lenght = lenght + 1
                            elseif buff(temp_offset, 1):string() == " " then
                               STATE = STATE_USER
                               msg_tree = temp_tree:add(buff(begin, lenght),"Message ID: " 
                                                        .. buff(begin, lenght):string())
                               begin = 0
                               lenght = 0
                            else
                                lenght = lenght + 1
                            end
                        elseif STATE == STATE_USER then
                            begin, lenght, STATE = textEval(buff, temp_offset, begin, lenght, " ",
                                                            STATE_USER, STATE_SUB, "To: ", msg_tree)
                        elseif STATE == STATE_SUB then
                            begin, lenght, STATE = textEval(buff, temp_offset, begin, lenght, ")",
                                                            STATE_SUB, STATE_MSG_BLOCK, "Subject: ", msg_tree)
                        end
                    end 
                end 
            elseif buff(offset, LEN_LOGGED_OUT):string() == "logged out" then
                lenght = pktlen - offset - SKIP_PARENTHESE - SKIP_QUOTE_MARK
                subtree:add(buff(offset, lenght),"Type: " .. buff(offset, lenght):string())

            elseif buff(offset, LEN_MSG_SEND):string() == "message sent" then
                lenght = pktlen - offset - SKIP_PARENTHESE - SKIP_QUOTE_MARK
                subtree:add(buff(offset, lenght),"Type: " .. buff(offset, lenght):string())

            elseif buff(offset, LEN_LOGGED_IN):string() == "user logged in" then
                temp_tree = subtree:add(buff(offset, LEN_LOGGED_IN),"Type: " .. buff(offset, LEN_LOGGED_IN):string())

                offset = offset + LEN_REGISTERED + SKIP_SPACE + SKIP_QUOTE_MARK
                lenght = pktlen - offset - SKIP_PARENTHESE - SKIP_QUOTE_MARK

                local b64_decode = buff(offset, lenght):bytes()
                b64_decode = b64_decode:base64_decode()
                temp_tree:add(buff(offset, lenght),"User name: " .. b64_decode:raw())
            
            elseif buff(offset,LEN_REGISTERED):string() == "registered user" then
                temp_tree = subtree:add(buff(offset, LEN_REGISTERED),"Type: " .. buff(offset, LEN_REGISTERED):string())
                
                offset = offset + LEN_REGISTERED + SKIP_SPACE
                lenght = pktlen - offset - SKIP_PARENTHESE - SKIP_QUOTE_MARK
                temp_tree:add(buff(offset, lenght), "User name: " .. buff(offset, lenght):string())
            else 
                return RET_ERR
            end
        else
            offset = offset + SKIP_QUOTE_MARK
            lenght = pktlen - offset - SKIP_PARENTHESE - SKIP_QUOTE_MARK
            subtree:add(buff(offset, lenght),"Type: " .. buff(offset, lenght):string())
        end

    else 

       local command
       local temp_offset
       local temp_tree = subtree
       local begin = 0
       local lenght = 0
       local STATE = STATE_READ
       local text_block_num
       local msg_in_block = {}
       local msg_delim = {}

       temp_offset = offset + 1

        if buff(temp_offset,LEN_LIST):string() == "list" then
            command = "list"
	        subtree:add(buff(temp_offset,LEN_LIST),"Command: " .. buff(temp_offset,LEN_LIST):string())
            temp_offset = LEN_LIST
            text_block_num = 1
            msg_in_block = { [0] = "User name: " }
            msg_delim = { [0] = ")" }
        elseif buff(temp_offset,LEN_SEND):string() == "send" then
            command = "send"
	        subtree:add(buff(temp_offset,LEN_SEND),"Command: " .. buff(temp_offset,LEN_SEND):string())
            temp_offset = LEN_SEND
            text_block_num = 4
            msg_in_block = { [0] = "From: ", [1] = "To: ", [2] = "Subject: ", [3] = "Text: " }
            msg_delim = { [0] = " ", [1] = " ", [2] = " ", [3] = ")" }
	    elseif buff(temp_offset,LEN_FETCH):string() == "fetch" then
            command = "fetch"
	        subtree:add(buff(temp_offset,LEN_FETCH),"Command: " .. buff(temp_offset,LEN_FETCH):string())
            temp_offset = LEN_FETCH
            text_block_num = 1
            msg_in_block = { [0] = "User name: ", [1] = "Message ID: "}
            msg_delim = { [0] = " " }
        elseif buff(temp_offset,LEN_LOGIN):string() == "login" then
            command = "login"
	        subtree:add(buff(temp_offset,LEN_LOGIN),"Command: " .. buff(temp_offset,LEN_LOGIN):string())
            temp_offset = LEN_LOGIN
            text_block_num = 2
            msg_in_block = { [0] = "User name: ", [1] = "Password: " }
            msg_delim = { [0] = " ", [1] = ")" }
        elseif buff(temp_offset,LEN_LOGOUT):string() == "logout" then
            command = "logout"
	        subtree:add(buff(temp_offset,LEN_LOGOUT),"Command: " .. buff(temp_offset,LEN_LOGOUT):string())
            temp_offset = LEN_LOGOUT
            text_block_num = 1
            msg_in_block = { [0] = "User name: " }
            msg_delim = { [0] = ")" }
        elseif buff(temp_offset,LEN_REGISTER):string() == "register" then
            command = "register"
	        subtree:add(buff(temp_offset,LEN_REGISTER),"Command: " .. buff(temp_offset,LEN_REGISTER):string())
            temp_offset = LEN_REGISTER
            text_block_num = 2
            msg_in_block = { [0] = "User name: ", [1] ="Password: " }
            msg_delim = { [0] = " ", [1] = ")" }
        else 
            return RET_ERR
        end

        pinfo.cols.info = "Request: " .. command

        temp_offset = temp_offset + SKIP_SPACE
        local curr_block = 0
        while (text_block_num ~= curr_block) do 
            temp_offset = temp_offset + 1
            
            if STATE == STATE_READ then
                begin, lenght, STATE = textEval(buff, temp_offset, begin, lenght, msg_delim[curr_block],
                                                STATE_READ, STATE_STOP, msg_in_block[curr_block], temp_tree)
            elseif STATE == STATE_STOP then
                curr_block = curr_block + 1
                temp_offset = temp_offset - SKIP_SPACE
                STATE = STATE_READ
            end

        end 

    end

    pinfo.cols.protocol = isa_proto.name

    return 1
end

evalStatus = function(buff, pinfo, tree)

    if buff(1,2):string() == "ok" then
        status = "ok"
        lenght = 2
    elseif buff(1,3):string() == "err" then
        status = "err"
        lenght = 3
    else
        return RET_ERR
    end

    tree:add(buff(1,lenght), "Status: " .. buff(1, lenght):string())
    pinfo.cols.info = "Response: " .. status

    return status, 1 + lenght + SKIP_SPACE

end

textEval = function(buff, offset, begin, lenght, end_mark, curr_state, next_state, msg, tree)
    state = curr_state
    if begin == 0 then
        begin = offset + SKIP_QUOTE_MARK
    elseif buff(offset, 1):string() == end_mark and buff(offset-1, 1):string() ~= "\\" then
        state = next_state
        tree:add(buff(begin, lenght-SKIP_QUOTE_MARK), msg 
                        .. buff(begin, lenght-SKIP_QUOTE_MARK):string())
        begin = 0
        lenght = 0
    else
        lenght = lenght + 1
    end

    return begin, lenght, state
end 


local tcp_port = DissectorTable.get("tcp.port")
tcp_port:add(50000, isa_proto)