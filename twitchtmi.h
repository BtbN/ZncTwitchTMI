#pragma once

#include <znc/Modules.h>
#include <znc/Threads.h>

#include <unordered_set>
#include <functional>
#include <vector>
#include <ctime>
#include <list>

class TwitchTMIUpdateTimer;

class TwitchTMI : public CModule
{
	friend class TwitchTMIUpdateTimer;
	friend class TwitchTMIJob;

	public:
	MODCONSTRUCTOR(TwitchTMI) { lastFrankerZ = 0; }
	virtual ~TwitchTMI();

	bool OnLoad(const CString &sArgsi, CString &sMessage) override;
	bool OnBoot() override;

	void OnIRCConnected() override;

	CModule::EModRet OnUserRaw(CString &sLine) override;
	CModule::EModRet OnRawMessage(CMessage &msg) override;
	CModule::EModRet OnUserJoin(CString &sChannel, CString &sKey) override;
	CModule::EModRet OnPrivTextMessage(CTextMessage &Message) override;
	CModule::EModRet OnChanTextMessage(CTextMessage &Message) override;
	CModule::EModRet OnUserTextMessage(CTextMessage &msg) override;
	bool OnServerCapAvailable(const CString &sCap) override;

	private:
	void PutUserChanMessage(CChan *chan, const CString &format, const CString &text);

	private:
	TwitchTMIUpdateTimer *timer;
	std::time_t lastFrankerZ;
	std::unordered_set<CString> liveChannels;
};

class TwitchTMIUpdateTimer : public CTimer
{
	friend class TwitchTMI;

	public:
	TwitchTMIUpdateTimer(TwitchTMI *mod);

	private:
	void RunJob() override;

	private:
	TwitchTMI *mod;
};

class TwitchTMIJob : public CJob
{
	public:
	TwitchTMIJob(TwitchTMI *mod, const std::list<CString> &channels):mod(mod),channels(channels) {}

	void runThread() override;
	void runMain() override;

	private:
	TwitchTMI *mod;
	std::list<CString> channels;
	std::vector<CString> titles;
	std::vector<bool> lives;
};

class GenericJob : public CJob
{
	public:
	GenericJob(std::function<void()> threadFunc, std::function<void()> mainFunc):threadFunc(threadFunc),mainFunc(mainFunc) {}

	void runThread() override { threadFunc(); }
	void runMain() override { mainFunc(); }

	private:
	std::function<void()> threadFunc;
	std::function<void()> mainFunc;
};

