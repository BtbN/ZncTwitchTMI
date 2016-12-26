#include <znc/main.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/Nick.h>
#include <znc/Chan.h>
#include <znc/IRCSock.h>
#include <znc/version.h>

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
		for(CChan *ch: GetNetwork()->GetChans())
		{
			ch->SetTopic(CString());

			CString chname = ch->GetName().substr(1);
			CThreadPool::Get().addJob(new TwitchTMIJob(this, chname));
		}
	}

	if(GetArgs().Token(0) != "FrankerZ")
		lastFrankerZ = std::numeric_limits<decltype(lastFrankerZ)>::max();

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

CModule::EModRet TwitchTMI::OnUserRaw(CString &sLine)
{
	if(sLine.Left(5).Equals("WHO #"))
		return CModule::HALT;

	if(sLine.Left(5).Equals("AWAY "))
		return CModule::HALT;

	if(sLine.Left(12).Equals("TWITCHCLIENT"))
		return CModule::HALT;

	if(sLine.Left(9).Equals("JTVCLIENT"))
		return CModule::HALT;

	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnRawMessage(CMessage &msg)
{
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
		//TODO: Translate Tags to useful message
		msg.SetCommand("PRIVMSG");
		msg.SetParam(1, "<<<RESUB>>> " + msg.GetParam(1));
	}

	CString realNick = msg.GetTag("display-name").Trim_n();
	if(realNick != "")
		msg.GetNick().SetNick(realNick);

	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnUserJoin(CString& sChannel, CString& sKey)
{
	CString chname = sChannel.substr(1);
	CThreadPool::Get().addJob(new TwitchTMIJob(this, chname));

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

CModule::EModRet TwitchTMI::OnChanTextMessage(CTextMessage &Message)
{
	if(Message.GetNick().GetNick().Equals("jtv"))
		return CModule::HALT;

	if(Message.GetText() == "FrankerZ" && std::time(nullptr) - lastFrankerZ > 10)
	{
		std::stringstream ss;
		CString mynick = GetNetwork()->GetIRCNick().GetNickMask();
		CChan *chan = Message.GetChan();

		ss << "PRIVMSG " << chan->GetName() << " :FrankerZ";
		PutIRC(ss.str());

		CThreadPool::Get().addJob(new GenericJob([]() {}, [this, chan, mynick]()
		{
			PutUserChanMessage(chan, mynick, "FrankerZ");
		}));

		lastFrankerZ = std::time(nullptr);
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
	:CTimer(tmod, 30, 0, "TwitchTMIUpdateTimer", "Downloads Twitch information")
	,mod(tmod)
{
}

void TwitchTMIUpdateTimer::RunJob()
{
	if(!mod->GetNetwork())
		return;

	for(CChan *chan: mod->GetNetwork()->GetChans())
	{
		CString chname = chan->GetName().substr(1);
		CThreadPool::Get().addJob(new TwitchTMIJob(mod, chname));
	}
}


void TwitchTMIJob::runThread()
{
	std::stringstream ss, ss2;
	ss << "https://api.twitch.tv/kraken/channels/" << channel;
	ss2 << "https://api.twitch.tv/kraken/streams/" << channel;

	CString url = ss.str();
	CString url2 = ss2.str();

	Json::Value root = getJsonFromUrl(url.c_str(), "Accept: application/vnd.twitchtv.v3+json");
	Json::Value root2 = getJsonFromUrl(url2.c_str(), "Accept: application/vnd.twitchtv.v3+json");

	if(!root.isNull())
	{
		Json::Value &titleVal = root["status"];
		title = CString();

		if(!titleVal.isString())
			titleVal = root["title"];

		if(titleVal.isString())
		{
			title = titleVal.asString();
			title.Trim();
		}
	}

	live = false;

	if(!root2.isNull())
	{
		Json::Value &streamVal = root2["stream"];

		if(!streamVal.isNull())
			live = true;
	}
}

void TwitchTMIJob::runMain()
{
	if(title.empty())
		return;

	CChan *chan = mod->GetNetwork()->FindChan(CString("#") + channel);

	if(!chan)
		return;

	if(chan->GetTopic() != title)
	{
		chan->SetTopic(title);
		chan->SetTopicOwner("jtv");
		chan->SetTopicDate((unsigned long)time(nullptr));

		std::stringstream ss;
		ss << ":jtv TOPIC #" << channel << " :" << title;

		mod->PutUser(ss.str());
	}

	auto it = mod->liveChannels.find(channel);
	if(it != mod->liveChannels.end())
	{
		if(!live)
		{
			mod->liveChannels.erase(it);
			mod->PutUserChanMessage(chan, "jtv", ">>> Channel just went offline! <<<");
		}
	}
	else
	{
		if(live)
		{
			mod->liveChannels.insert(channel);
			mod->PutUserChanMessage(chan, "jtv", ">>> Channel just went live! <<<");
		}
	}
}


template<> void TModInfo<TwitchTMI>(CModInfo &info)
{
	info.SetWikiPage("Twitch");
	info.SetHasArgs(true);
}

NETWORKMODULEDEFS(TwitchTMI, "Twitch IRC helper module")
