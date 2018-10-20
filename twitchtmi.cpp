#include <znc/main.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/Nick.h>
#include <znc/Chan.h>
#include <znc/IRCSock.h>
#include <znc/version.h>

#include <mutex>
#include <limits>

#include "twitchtmi.h"
#include "jload.h"

#if (VERSION_MAJOR < 1) || (VERSION_MAJOR == 1 && VERSION_MINOR < 7)
#error This module needs at least ZNC 1.7.0 or later.
#endif

TwitchTMI::~TwitchTMI()
{

}

bool TwitchTMI::OnLoad(const CString& sArgsi, CString& sMessage)
{
    OnBoot();

    if(GetNetwork())
    {
        std::list<CString> channels;

        for(CChan *ch: GetNetwork()->GetChans())
        {
            CString sname = ch->GetName().substr(1);
            sname.MakeLower();
            channels.push_back(sname);

            ch->SetTopic(GetNV("tmi_topic_" + sname));
            ch->SetTopicOwner("jtv");
            ch->SetTopicDate(GetNV("tmi_topic_time_" + sname).ToULong());
        }

        CThreadPool::Get().addJob(new TwitchTMIJob(this, channels));
    }

    if(GetArgs().Token(0) != "FrankerZ") {
        lastFrankerZ = std::numeric_limits<decltype(lastFrankerZ)>::max();
        noLastPlay = true;
    }

    PutIRC("CAP REQ :twitch.tv/membership");
    PutIRC("CAP REQ :twitch.tv/commands");
    PutIRC("CAP REQ :twitch.tv/tags");

    return true;
}

bool TwitchTMI::OnBoot()
{
    initCurl();

    timer = new TwitchTMIUpdateTimer(this);
    AddTimer(timer);

    return true;
}

void TwitchTMI::OnIRCConnected()
{
    PutIRC("CAP REQ :twitch.tv/membership");
    PutIRC("CAP REQ :twitch.tv/commands");
    PutIRC("CAP REQ :twitch.tv/tags");
}

CModule::EModRet TwitchTMI::OnUserRawMessage(CMessage &msg)
{
    const CString &cmd = msg.GetCommand();
    if(cmd.Equals("WHO") || cmd.Equals("AWAY") || cmd.Equals("TWITCHCLIENT") || cmd.Equals("JTVCLIENT"))
    {
        return CModule::HALT;
    }
    else if(cmd.Equals("KICK"))
    {
        VCString params = msg.GetParams();
        if(params.size() < 2)
            return CModule::HALT;
        msg.SetCommand("PRIVMSG");
        params = { params[0], "/timeout " + CString(" ").Join(std::next(params.begin()), params.end()) };
        msg.SetParams(params);
    }
    else if(cmd.Equals("MODE"))
    {
        VCString params = msg.GetParams();
        if(params.size() != 3)
            return CModule::HALT;

        CString victim, mode = params[1];

        CNick victimNick;
        victimNick.Parse(params[2]);

        if(mode.Equals("+b"))
            mode = "/ban";
        else if(mode.Equals("-b"))
            mode = "/unban";
        else if(mode.Equals("+o"))
            mode = "/mod";
        else if(mode.Equals("-o"))
            mode = "/unmod";
        else
        {
            PutStatus("Unknown mode change: " + mode);
            return CModule::HALT;
        }

        if(victimNick.GetHost().Contains(".tmi.twitch.tv"))
            victim = victimNick.GetHost().Token(0, false, ".");
        else
            victim = victimNick.GetNick();

        msg.SetCommand("PRIVMSG");
        params = { params[0], mode + " " + victim };
        msg.SetParams(params);
    }

    return CModule::CONTINUE;
}

static CString subPlanToName(const CString &plan)
{
    if(plan.AsLower() == "prime")
        return "Twitch Prime";
    else if(plan == "1000")
        return "Tier 1";
    else if(plan == "2000")
        return "Tier 2";
    else if(plan == "3000");
        return "Tier 3";
    return plan;
}

CModule::EModRet TwitchTMI::OnRawMessage(CMessage &msg)
{
    CString realNick = msg.GetTag("display-name").Trim_n();
    if (realNick == "")
        realNick = msg.GetNick().GetNick();

    if(msg.GetCommand().Equals("HOSTTARGET"))
    {
        return CModule::HALT;
    }
    else if(msg.GetCommand().Equals("CLEARCHAT"))
    {
        msg.SetCommand("NOTICE");
        if(msg.GetParam(1) != "")
        {
            msg.SetParam(1, msg.GetParam(1) + " was timed out.");
        }
        else
        {
            msg.SetParam(1, "Chat was cleared by a moderator.");
        }
    }
    else if(msg.GetCommand().Equals("USERSTATE"))
    {
        return CModule::HALT;
    }
    else if(msg.GetCommand().Equals("ROOMSTATE"))
    {
        return CModule::HALT;
    }
    else if(msg.GetCommand().Equals("RECONNECT"))
    {
        return CModule::HALT;
    }
    else if(msg.GetCommand().Equals("GLOBALUSERSTATE"))
    {
        return CModule::HALT;
    }
    else if(msg.GetCommand().Equals("WHISPER"))
    {
        msg.SetCommand("PRIVMSG");
    }
    else if(msg.GetCommand().Equals("USERNOTICE"))
    {
        CString msg_id = msg.GetTag("msg-id");
        CString new_msg = "<unknown usernotice " + msg_id + "> from " + realNick;
        if(msg_id == "sub" || msg_id == "resub") {
            CString dur = msg.GetTag("msg-param-months");
            CString plan = subPlanToName(msg.GetTag("msg-param-sub-plan"));
            CString txt = msg.GetParam(1).Trim_n();

            new_msg = realNick + " just ";
            if(msg_id == "sub")
                new_msg += "subscribed with a " + plan + " sub";
            else
                new_msg += "resubscribed with a " + plan + " sub for " + dur + " months";

            if(txt != "")
                new_msg += " saying: " + txt;
            else
                new_msg += "!";

            realNick = "jtv";
        } else if(msg_id == "subgift") {
            CString rec = msg.GetTag("msg-param-recipient-display-name");
            CString plan = subPlanToName(msg.GetTag("msg-param-sub-plan"));

            new_msg = realNick + " gifted a " + plan + " sub to " + rec + "!";
        } else if(msg_id == "raid") {
            CString cnt = msg.GetTag("msg-param-viewerCount");
            CString src = msg.GetTag("msg-param-displayName");

            new_msg = src + " is raiding with a party of " + cnt + "!";
        } else if(msg_id == "ritual") {
            CString rit = msg.GetTag("msg-param-ritual-name");
            CString txt = msg.GetParam(1).Trim_n();

            new_msg = "<unknown ritual " + rit + ">";
            if(rit == "new_chatter")
                new_msg = realNick + " is new here, saying: " + txt;
        } else if(msg_id == "giftpaidupgrade") {
            new_msg = msg.GetTag("system-msg");
        }

        realNick = "ttv";
        new_msg = ">>> " + new_msg + " <<<";
        msg.SetCommand("PRIVMSG");
        msg.SetParam(1, new_msg);
    }

    msg.GetNick().SetNick(realNick);

    if(realNick == "ttv")
        msg.GetNick().Parse("ttv!ttv@ttv.tmi.twitch.tv");

    return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnUserJoin(CString& sChannel, CString& sKey)
{
    return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnPrivTextMessage(CTextMessage &Message)
{
    if(Message.GetNick().GetNick().Equals("jtv"))
        return CModule::HALT;

    return CModule::CONTINUE;
}

void TwitchTMI::PutUserChanMessage(CChan *chan, const CString &from, const CString &msg)
{
    std::stringstream ss;
    ss << ":" << from << " PRIVMSG " << chan->GetName() << " :";
    CString s = ss.str();

    PutUser(s + msg);

    if(!chan->AutoClearChanBuffer() || !GetNetwork()->IsUserOnline() || chan->IsDetached())
        chan->AddBuffer(s + "{text}", msg);
}

void TwitchTMI::InjectMessageHelper(CChan *chan, const CString &action)
{
    std::stringstream ss;
    CString mynick = GetNetwork()->GetIRCNick().GetNickMask();

    ss << "PRIVMSG " << chan->GetName() << " :" << action;
    PutIRC(ss.str());

    AddJob(new GenericJob(this, "put_msg_job_" + chan->GetName(), "put msg helper", []() {}, [this, chan, mynick, action]()
    {
        PutUserChanMessage(chan, mynick, action);
    }));
}

CModule::EModRet TwitchTMI::OnChanTextMessage(CTextMessage &Message)
{
    if(Message.GetNick().GetNick().Equals("jtv"))
        return CModule::HALT;

    if(Message.GetNick().GetNick().AsLower().Equals("hentaitheace"))
        return CModule::CONTINUE;

    CChan *chan = Message.GetChan();
    const CString &cname = chan->GetName();

    if(Message.GetText() == "FrankerZ" && std::time(nullptr) - lastFrankerZ > 10)
    {
        InjectMessageHelper(chan, "FrankerZ");
        lastFrankerZ = std::time(nullptr);
    }
    else if(Message.GetText() == "!play" && !noLastPlay && (!lastPlay.count(cname) || std::time(nullptr) - lastPlay[cname] > 135))
    {
        AddTimer(new GenericTimer(this, 10, 1, "play_timer_" + cname, "Writes !play in 15 sec!", [this, chan]()
        {
            InjectMessageHelper(chan, "!play");
        }));
        lastPlay[cname] = std::time(nullptr);
    }

    return CModule::CONTINUE;
}

bool TwitchTMI::OnServerCapAvailable(const CString &sCap)
{
    if(sCap == "twitch.tv/membership")
        return true;
    else if(sCap == "twitch.tv/tags")
        return true;
    else if(sCap == "twitch.tv/commands")
        return true;

    return false;
}

CModule::EModRet TwitchTMI::OnUserTextMessage(CTextMessage &msg)
{
    if(msg.GetTarget().Left(1).Equals("#"))
        return CModule::CONTINUE;

    msg.SetText(msg.GetText().insert(0, " ").insert(0, msg.GetTarget()).insert(0, "/w "));
    msg.SetTarget("#jtv");

    return CModule::CONTINUE;
}


TwitchTMIUpdateTimer::TwitchTMIUpdateTimer(TwitchTMI *tmod)
    :CTimer(tmod, 10, 0, "TwitchTMIUpdateTimer", "Downloads Twitch information")
    ,mod(tmod)
{
}

void TwitchTMIUpdateTimer::RunJob()
{
    if(!mod->GetNetwork())
        return;

    std::list<CString> channels;

    for(CChan *chan: mod->GetNetwork()->GetChans())
    {
        channels.push_back(chan->GetName().substr(1));
    }

    mod->AddJob(new TwitchTMIJob(mod, channels));
}

void TwitchTMIJob::runThread()
{
    std::unique_lock<std::mutex> lock_guard(mod->job_thread_lock, std::try_to_lock);

    if(!lock_guard.owns_lock() || !channels.size())
    {
        channels.clear();
        return;
    }

    Json::Value root = getJsonFromUrl(
        "https://api.twitch.tv/gql",
        { "Content-Type: application/json" },
        "{ \"variables\": { \"logins\": [\"" + CString("\", \"").Join(channels.begin(), channels.end()) + "\"] },"
        " \"query\": \"query user_status($logins: [String!]){ users(logins: $logins) { broadcastSettings{ game{ name } title } login stream{ type } } }\" }");
    if(root.isNull())
        return;

    Json::Value data;
    try
    {
        data = root["data"];
        if(!data.isObject())
            return;
        data = data["users"];
        if(!data.isArray())
            return;
    } catch(const Json::Exception&) {
        return;
    }

    for(Json::ArrayIndex i = 0; i < data.size(); i++)
    {
        try
        {
            Json::Value val = data[i];
            if(!val.isObject())
                continue;

            CString channel = val["login"].asString();
            CString title = val["broadcastSettings"]["title"].asString();
            CString type = val["stream"]["type"].asString();

            CString game;
            Json::Value gameVal = val["broadcastSettings"]["game"];
            if(gameVal.isObject())
                game = gameVal.get("name", "").asString();

            channel.MakeLower();

            if(!game.Equals(""))
                title += " [" + game + "]";

            bool isLive = !type.Equals("");
            lives[channel] = isLive;

            if(isLive && !type.Equals("live"))
                title += " [" + type.MakeUpper() + "]";
            else if(!isLive)
                title += " [OFFLINE]";

            titles[channel] = title;
        } catch(const Json::Exception&) {
            continue;
        }
    }
}

void TwitchTMIJob::runMain()
{
    for(CString channel: channels)
    {
        CChan *chan = mod->GetNetwork()->FindChan(CString("#") + channel);
        if(!chan)
            continue;
        channel.MakeLower();

        CString title;
        if(titles.count(channel))
            title = titles[channel];
        else
            title = mod->GetNV("tmi_topic_" + channel);

        if(!title.empty() && chan->GetTopic() != title)
        {
            unsigned long topic_time = (unsigned long)time(nullptr);
            chan->SetTopic(title);
            chan->SetTopicOwner("jtv");
            chan->SetTopicDate(topic_time);

            std::stringstream ss;
            ss << ":jtv TOPIC #" << channel << " :" << title;

            mod->PutUser(ss.str());

            mod->SetNV("tmi_topic_" + channel, title, true);
            mod->SetNV("tmi_topic_time_" + channel, CString(topic_time), true);
        }

        auto it = mod->liveChannels.find(channel);
        if(it != mod->liveChannels.end())
        {
            if(!lives.count(channel) || !lives[channel])
            {
                mod->liveChannels.erase(it);
                mod->PutUserChanMessage(chan, "jtv", ">>> Channel just went offline! <<<");
            }
        }
        else
        {
            if(lives.count(channel) && lives[channel])
            {
                mod->liveChannels.insert(channel);
                mod->PutUserChanMessage(chan, "jtv", ">>> Channel just went live! <<<");
            }
        }
    }
}


template<> void TModInfo<TwitchTMI>(CModInfo &info)
{
    info.SetWikiPage("Twitch");
    info.SetHasArgs(true);
}

NETWORKMODULEDEFS(TwitchTMI, "Twitch IRC helper module")
