-- author: Alexandr Chalupnik
-- email: xchalu15@stud.fit.vutbr.cz
-- file: isa.lua

ISA_PORT = 32323  -- implicitni port protokolu

RET_SUCC = 1
RET_ERR = 0

MSG_OK = 1  -- zprava je kompletni
MSG_MORE_SEGMENT = 0  -- je pozadovan dalsi segment

SHIFT_BYTE = 1  -- posun o 1 byte,
                -- +SHIFT_BYTE posun doprava
                -- -SHIFT_BYTE posun doleva

STATUS_OK = 1  -- (ok ...)
STATUS_ERR = 0 -- (err ...)

STATE_ESC_CHAR = 2
STATE_READ = 1
STATE_STOP = 0

-- delky prikazu
LEN_LIST = 4
LEN_SEND = 4
LEN_FETCH = 5
LEN_LOGIN = 5
LEN_LOGOUT = 6
LEN_REGISTER = 8

-- delky zprav u odpovedi
LEN_LOGGED_OUT = 10
LEN_MSG_SEND = 12
LEN_LOGGED_IN = 14
LEN_REGISTERED = 15

local isa_proto = Proto("ISA", "ISA PROTOCOL")

function isa_proto.dissector(buff, pinfo, tree)
    -- spusteni dissectoru
    -- vraci RET_SUCC pri uspesnem zpracovani
    -- jinak RET_ERR

    local pktlen = buff:len()

    local bytes_consumed = 0
    while bytes_consumed < pktlen do
        local read = dissectISA(buff, pinfo, tree, bytes_consumed)
        -- 0 = chyba = RET_ERR
        -- <0 = DESEGMENT_ONE_MORE_SEGMENT
        -- 1 = RET_SUCC

        if read == RET_SUCC then
            -- prectena cela zprava
            bytes_consumed = bytes_consumed + pktlen
        elseif result == RET_ERR then
            -- pri cteni doslo k chybe
            return RET_ERR
        else
            -- zprava neni kompletni, potrebujeme dalsi segment
            -- -read == DESEGMENT_ONE_MORE_SEGMENT
            pinfo.desegment_len = -read
            return RET_SUCC
        end
    end

    return RET_SUCC
end

dissectISA = function(buff, pinfo, tree, offset)

    local ret = check_segment(buff, offset)

    -- 0 = RET_ERR
    -- <0 = DESEGMENT_ONE_MORE_SEGMENT
    -- 1 = MSG_OK = RET_SUCC
    if ret <= 0 then
        return ret
    end

    if buff(0,1):string() ~= "(" then
        return RET_ERR
    end

    local subtree = tree:add(isa_proto, buff(), "ISA Protocol Data")
    pinfo.cols.protocol = isa_proto.name

    if pinfo.src_port == ISA_PORT then
        -- RESPONSE
        ret = parse_response(buff, pinfo, subtree)
    else
        -- REQUEST
        ret = parse_request(buff, pinfo, subtree)
    end

    return ret
end

check_segment = function (buff, offset)
    -- funkce overi zda je zprava kompletni,
    -- nebo potrebujeme dalsi segment
    -- vraci RET_ERR pri chybe,
    --       RET_SUCC pri kompletni zprave
    --       -DESEGMENT_ONE_MORE_SEGMENT pro dalsi segment

    local remaining = buff:len() - offset

    if remaining ~= buff:reported_length_remaining(offset) then
        return ERR
    end

    if check_message(buff) == MSG_MORE_SEGMENT then
        return -DESEGMENT_ONE_MORE_SEGMENT
    end

    return RET_SUCC
end

check_message = function(buff)
    -- funkce prochazi cely buffer,
    -- a overuje pritomnost pocatecni/koncove zavorky
    -- pokud pocet zavorek neni 0,
    -- zprava neni kompletni a potrebujeme dalsi segment
    -- vraci MSG_OK pokud je v buff cela zprava
    -- jinak MSG_MORE_SEGMENT pro cteni dalsiho segmentu

    -- stavy pro fsm
    local QM_OPENED = 10  -- pocatecni uvozovka
    local QM_CLOSED = 11  -- koncova uvozovka

    local par_cnt = 0 -- pocet zavorek
    local state = QM_CLOSED -- stav

    local pktlen = buff:len()
    local index = 0
    while index < pktlen do
        -- pokud je stav QM_OPENED, zavorky se ignoruji

        if state == QM_CLOSED and buff(index, 1):string() == "(" then
            par_cnt = par_cnt + 1
        elseif state == QM_CLOSED and buff(index, 1):string() == ")" then
            par_cnt = par_cnt - 1
        elseif state == QM_CLOSED and buff(index, 1):string() == "\"" then
            state = QM_OPENED
        elseif state == QM_OPENED and buff(index, 1):string() == "\"" then
            state = QM_CLOSED
        end

        index = index + 1
    end

    if par_cnt == 0 then
        -- v pripade ze je pocet zavorek stejny,
        -- novy segment neni nutny
        return MSG_OK
    end

    return MSG_MORE_SEGMENT
end

eval_status = function(buff, pinfo, tree)
    -- funkce vyhodnoti typ zpravy
    -- vraci STATUS_ERR pro (err ...) nebo
    -- STATUS_OK pro (ok ...)
    -- a index prvni mezery za typem (offset)

    local status, offset
    -- offset je index na mezeru za typem
    if buff(1,2):string() == "ok" then
        status = STATUS_OK
        offset = 3
    elseif buff(1,3):string() == "err" then
        status = STATUS_ERR
        offset = 4
    end

    tree:add(buff(1,offset-SHIFT_BYTE),
            "Status: " .. buff(1, offset-SHIFT_BYTE):string())

    return status, offset
end

parse_response = function(buff, pinfo, tree)
    -- funkce vyhodnoti odpoved serveru
    -- a vypise parametry

    status, offset = eval_status(buff, pinfo, tree)
    local temp_tree -- pro vytvoreni nove urovne ve vypisu
    local cmd_params = {} -- parametry prikazu
    local pktlen = buff:len()
    local lenght

    pinfo.cols.info = "Response: "
                        .. ((status == STATUS_OK) and "OK" or "ERR")

    offset = offset + SHIFT_BYTE -- posun na znak " nebo (
    if status == STATUS_OK then
        lenght = pktlen - SHIFT_BYTE - offset
        if buff(offset, 1):string() == "(" then
            -- fetch nebo list
            if buff(offset + SHIFT_BYTE, 1):string() == "\"" then
                -- fetch
                lenght = pktlen - SHIFT_BYTE - offset
                temp_tree = tree:add(buff(offset, lenght),"Type: fetch")
                cmd_params = { "To: ", "Subject: ", "Text: " }

                offset = offset + SHIFT_BYTE -- posun na "
                local curr_param = 1
                while (curr_param <= #cmd_params) do
                    offset = param_eval(buff, offset, temp_tree, cmd_params[curr_param])
                    curr_param = curr_param + 1

                    if curr_param <= #cmd_params then
                        offset = offset + (2*SHIFT_BYTE)
                    end
                end
            elseif buff(offset, 1):string() == "(" then
                -- list
                tree = tree:add(buff(offset, lenght),"Type: messages")
                if buff(offset + SHIFT_BYTE, 1):string() == ")" then
                    tree:add(buff(),"Number of messages: 0")
                else
                    cmd_params = { "Message ID: ", "To: ", "Subject: " }
                    offset = offset + SHIFT_BYTE -- posun na (

                    local par_cnt = 1 -- pocet zavorek
                    local STATE = STATE_READ
                    while par_cnt ~= 0 do
                        -- cyklus skonci az bude sudy pocet (,)
                        temp_tree = tree

                        local curr_param = 1
                        if buff(offset, 1):string() == "(" then
                            par_cnt = par_cnt + 1
                            STATE = STATE_READ
                            -- otevira se blok s novymi parametry
                            -- (1 "..." "...")
                            -- ^
                        elseif buff(offset, 1):string() == ")" then
                            par_cnt = par_cnt - 1
                        end

                        while (curr_param <= #cmd_params
                                and STATE == STATE_READ) do
                            offset, subtree = param_eval(buff, offset,
                                                        temp_tree,
                                                        cmd_params[curr_param])
                            if curr_param <= #cmd_params then
                                if curr_param == 1 then
                                    -- message id:
                                    temp_tree = subtree
                                    offset = offset + SHIFT_BYTE
                                elseif curr_param == 2 then
                                    -- to:
                                    offset = offset + (2*SHIFT_BYTE)
                                end
                                --  po poslednim parametru
                                --  zustane offset pred uzavirajici )
                            end
                            curr_param = curr_param + 1
                        end
                        offset = offset + SHIFT_BYTE -- inkrementace offsetu
                        STATE = STATE_STOP -- zastavi vyhodnocovani parametru
                                           -- dokud se nenajdou nove
                    end
                end
            else
                return RET_ERR
            end

            return RET_SUCC
        end

        offset = offset + SHIFT_BYTE -- posun za "
        if buff(offset, LEN_LOGGED_OUT):string() == "logged out" then
            -- logout
            tree:add(buff(offset, LEN_LOGGED_OUT),
                        "Type: " .. buff(offset, LEN_LOGGED_OUT):string())

        elseif buff(offset, LEN_MSG_SEND):string() == "message sent" then
            -- send
            tree:add(buff(offset, LEN_MSG_SEND),
                        "Type: " .. buff(offset, LEN_MSG_SEND):string())

        elseif buff(offset, LEN_LOGGED_IN):string() == "user logged in" then
            -- login
            temp_tree = tree:add(buff(offset, LEN_LOGGED_IN),
                            "Type: " .. buff(offset, LEN_LOGGED_IN):string())
            -- posun na base64 login-token
            offset = offset + LEN_LOGGED_IN + (3*SHIFT_BYTE)
            lenght = pktlen - offset - (2*SHIFT_BYTE) -- kvuli ") na konci
            temp_tree:add(buff(offset, lenght),
                        "Login token: " .. buff(offset, lenght):string())

        elseif buff(offset,LEN_REGISTERED):string() == "registered user" then
            -- register
            temp_tree = tree:add(buff(offset, LEN_REGISTERED),
                        "Type: " .. buff(offset, LEN_REGISTERED):string())
            -- posun na user name
            offset = offset + LEN_REGISTERED + SHIFT_BYTE
            lenght = pktlen - offset - (2*SHIFT_BYTE) -- kvuli ") na konci
            temp_tree:add(buff(offset, lenght),
                            "User name: " .. buff(offset, lenght):string())
        else
            return RET_ERR
        end
    else -- ERR status
        offset = offset + SHIFT_BYTE -- posun za "
        lenght = pktlen - offset - (2*SHIFT_BYTE)
        -- 2*SHIFT_BYTE kvuli ") na konci retezce
        tree:add(buff(offset, lenght),
                    "Type: " .. buff(offset, lenght):string())
    end

    return RET_SUCC
end

parse_request = function(buff, pinfo, tree)
    -- funkce vyhodnoti pozadavek na server
    -- a vypise parametry

    local command -- prikaz
    local cmd_params = {} -- popis parametru prikazu
                          -- ktery se vypise
    local offset = 0

    offset = offset + SHIFT_BYTE -- preskoceni pocatecni (
    if buff(offset, LEN_LIST):string() == "list" then
        command = "list"
        tree:add(buff(offset, LEN_LIST), "Command: " .. command)
        offset = offset + LEN_LIST
        cmd_params = { "User token: " }

    elseif buff(offset, LEN_SEND):string() == "send" then
        command = "send"
        tree:add(buff(offset, LEN_SEND), "Command: " .. command)
        offset = offset + LEN_SEND
        cmd_params = { "From token: ", "To: ", "Subject: ", "Text: " }

    elseif buff(offset, LEN_FETCH):string() == "fetch" then
        command = "fetch"
        tree:add(buff(offset, LEN_FETCH), "Command: " .. command)
        offset = offset + LEN_FETCH
        cmd_params = { "User token: ", "Message ID: " }

    elseif buff(offset, LEN_LOGIN):string() == "login" then
        command = "login"
        tree:add(buff(offset, LEN_LOGIN), "Command: " .. command)
        offset = offset + LEN_LOGIN
        cmd_params = { "User name: ", "Password: " }

    elseif buff(offset, LEN_LOGOUT):string() == "logout" then
        command = "logout"
        tree:add(buff(offset, LEN_LOGOUT), "Command: " .. command)
        offset = offset + LEN_LOGOUT
        cmd_params = { "User token: " }

    elseif buff(offset, LEN_REGISTER):string() == "register" then
        command = "register"
        tree:add(buff(offset, LEN_REGISTER), "Command: " .. command)
        offset = offset + LEN_REGISTER
        cmd_params = { "User name: ", "Password: " }

    else
        return RET_ERR
    end

    offset = offset + SHIFT_BYTE
    local curr_param = 1
    while (curr_param <= #cmd_params) do
        offset = param_eval(buff, offset, tree, cmd_params[curr_param])
        curr_param = curr_param + 1

        if curr_param <= #cmd_params then
            offset = offset + SHIFT_BYTE
            if command ~= "fetch" then
                offset = offset + SHIFT_BYTE
            end
        end
    end

    pinfo.cols.info = "Request: " .. command

    return RET_SUCC
end

param_eval = function(buff, offset, tree, param)
    -- funkce vyhodnoti bezprostredne prvni parametry,
    -- ktery se nachazi v buff a vypise jej
    -- vraci pozici prvniho znaku, ktery se jiz nevypisuje
    --      pr.: obsah buff = "user2" "cHdkMg==")
    --                              ^ index ktery se vrati
    --      vypise se hodnota user2
    -- a subtree

    local begin = offset
    local begin_char = buff(offset, 1):string()
    local end_char -- znak ukoncujici parametr
    local STATE = STATE_READ

    -- buff(offset, 1) musi zacinat jednim z
    -- pocatecnich znaku, po kterych nasleduje parametr
    if begin_char == "(" then
        end_char = " "
    elseif begin_char == "\"" then
        end_char = "\""
    elseif begin_char == " " then
        end_char = ")"
    end

    repeat
        offset = offset + 1

        if STATE == STATE_ESC_CHAR then
            STATE = STATE_READ
            offset = offset + 1
        end

        if buff(offset, 1):string() == "\\" then
            STATE = STATE_ESC_CHAR
        end
    until (buff(offset, 1):string() == end_char and STATE ~= STATE_ESC_CHAR)

    begin = begin + SHIFT_BYTE
    local lenght = offset - begin
    local subtree = tree:add(buff(begin, lenght),
                            param .. buff(begin, lenght):string())

    return offset, subtree
end

local tcp_port = DissectorTable.get("tcp.port")
tcp_port:add(ISA_PORT, isa_proto)
