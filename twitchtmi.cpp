#include <znc/main.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/Nick.h>
#include <znc/Chan.h>

#include "twitchtmi.h"
#include "jload.h"


void TwitchTMI::init()
{
}

TwitchTMI::~TwitchTMI()
{

}

bool TwitchTMI::OnLoad(const CString& sArgsi, CString& sMessage)
{
	OnBoot();

	return true;
}

bool TwitchTMI::OnBoot()
{
	PutModule("OnLoad");

	if(GetNetwork())
	{
		for(CChan *chan: GetNetwork()->GetChans())
		{
			addChannel(chan->GetName());
		}

		if(GetNetwork()->IsIRCConnected())
			OnIRCConnected();
	}

	timer = new TwitchTMIUpdateTimer(this);
	AddTimer(timer);

	return true;
}

void TwitchTMI::addChannel(const CString& name)
{
	CString chname = name.substr(1);

	auto it = channels.find(chname);
	if(it == channels.end())
		channels[chname] = ChannelUserMap();
}

void TwitchTMI::remChannel(const CString& name)
{
	CString chname = name.substr(1);

	channels.erase(chname);
}

void TwitchTMI::OnIRCConnected()
{
	PutIRC("TWITCHCLIENT 3");
}


CModule::EModRet TwitchTMI::OnRaw(CString& sLine)
{
	if(sLine.Left(10).Equals(":jtv MODE "))
		return CModule::HALT;

	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnUserJoin(CString& sChannel, CString& sKey)
{
	addChannel(sChannel);
	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnUserPart(CString& sChannel, CString& sMessage)
{
	remChannel(sChannel);
	return CModule::CONTINUE;
}

CModule::EModRet TwitchTMI::OnPrivMsg(CNick& nick, CString& sMessage)
{
	if(!nick.GetNick().Equals("jtv", true))
		return CModule::CONTINUE;

	return CModule::HALT;
}

CModule::EModRet TwitchTMI::OnChanMsg(CNick& nick, CChan& channel, CString& sMessage)
{
	if(!nick.GetNick().Equals("jtv", true))
		return CModule::CONTINUE;

	return CModule::HALT;
}



TwitchTMIUpdateTimer::TwitchTMIUpdateTimer(TwitchTMI *tmod)
	:CTimer(tmod, 10, 0, "TwitchTMIUpdateTimer", "Downloads Twitch information")
	,mod(tmod)
{

}

void TwitchTMIUpdateTimer::RunJob()
{
	for(auto &chPair: mod->channels)
	{
		const CString &ch = chPair.first;
		ChannelUserMap &umap = chPair.second;

		std::string url = "https://tmi.twitch.tv/group/user/";
		url.append(ch).append("/chatters");

		Json::Value root = getJsonFromUrl(url.c_str());

		if(root.isNull())
			continue;

		const Json::Value &chatters = root["chatters"];

		if(chatters.isNull())
			continue;

		const Json::Value &viewers = chatters["viewers"];
		const Json::Value &admins = chatters["admins"];
		const Json::Value &staff = chatters["staff"];
		const Json::Value &moderators = chatters["moderators"];

		ChannelUserMap nmap;

		for(const Json::Value &user: viewers)
		{
			procUser(user.asString(), ch, 0, umap);
			nmap[user.asString()] = 0;
		}

		for(const Json::Value &user: moderators)
		{
			procUser(user.asString(), ch, 1, umap);
			nmap[user.asString()] = 1;
		}

		for(const Json::Value &user: admins)
		{
			procUser(user.asString(), ch, 2, umap);
			nmap[user.asString()] = 2;
		}

		for(const Json::Value &user: staff)
		{
			procUser(user.asString(), ch, 3, umap);
			nmap[user.asString()] = 3;
		}

		std::unordered_set<CString, std::hash<std::string> > leftUsers;

		for(const auto &pair: umap)
		{
			if(nmap.find(pair.first) == nmap.end())
				leftUsers.insert(pair.first);
		}

		for(const CString &user: leftUsers)
		{
			leaveUser(user, ch, umap);
		}
	}
}

void TwitchTMIUpdateTimer::procUser(const CString& name, const CString& channel, int level, ChannelUserMap& umap)
{
	auto it = umap.find(name);

	if(it == umap.end())
	{
		std::stringstream ss;
		ss << ":" << name << "!" << name << "@" << name << ".tmi.twitch.tv";
		ss << " JOIN #" << channel;

		mod->PutUser(ss.str());

		umap[name] = level;
	}
	else
	{
		int olevel = it->second;

		if(level != olevel)
		{

			umap[name] = level;
		}
	}
}

void TwitchTMIUpdateTimer::leaveUser(const CString& name, const CString& channel, ChannelUserMap& umap)
{
	umap.erase(name);

	std::stringstream ss;
	ss << ":" << name << "!" << name << "@" << name << ".tmi.twitch.tv";
	ss << " PART #" << channel;

	mod->PutUser(ss.str());
}



template<> void TModInfo<TwitchTMI>(CModInfo &info)
{
	info.SetWikiPage("Twitch");
	info.SetHasArgs(false);
}

NETWORKMODULEDEFS(TwitchTMI, "Twitch IRC helper module")
