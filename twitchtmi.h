#pragma once

#include <znc/Modules.h>
#include <unordered_set>
#include <unordered_map>


class TwitchTMIUpdateTimer;

typedef std::unordered_map<CString, int, std::hash<std::string>, std::equal_to<std::string> > ChannelUserMap;

class TwitchTMI : public CModule
{
	friend class TwitchTMIUpdateTimer;

	public:
	MODCONSTRUCTOR(TwitchTMI) { init(); }
	virtual ~TwitchTMI();

	virtual bool OnLoad(const CString &sArgsi, CString &sMessage);
	virtual bool OnBoot();

	virtual void OnIRCConnected();

	virtual CModule::EModRet OnRaw(CString &sLine);
	virtual CModule::EModRet OnUserJoin(CString &sChannel, CString &sKey);
	virtual CModule::EModRet OnUserPart(CString &sChannel, CString &sMessage);
	virtual CModule::EModRet OnPrivMsg(CNick &nick, CString &sMessage);
	virtual CModule::EModRet OnChanMsg(CNick &nick, CChan &channel, CString &sMessage);

	private:
	void init();
	void addChannel(const CString &name);
	void remChannel(const CString &name);

	private:
	std::unordered_map<CString, ChannelUserMap, std::hash<std::string>, std::equal_to<std::string> > channels;

	TwitchTMIUpdateTimer *timer;
};

class TwitchTMIUpdateTimer : public CTimer
{
	friend class TwitchTMI;

	public:
	TwitchTMIUpdateTimer(TwitchTMI *mod);

	private:
	virtual void RunJob();
	void procUser(const CString &name, const CString &channel, int level, ChannelUserMap &umap);
	void leaveUser(const CString &name, const CString &channel, ChannelUserMap& umap);

	private:
	TwitchTMI *mod;
};
