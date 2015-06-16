#pragma once

#include <znc/Modules.h>
#include <znc/Threads.h>
#include <functional>
#include <ctime>

class TwitchTMIUpdateTimer;

class TwitchTMI : public CModule
{
	friend class TwitchTMIUpdateTimer;
	friend class TwitchTMIJob;

	public:
	MODCONSTRUCTOR(TwitchTMI) { lastFrankerZ = 0; }
	virtual ~TwitchTMI();

	virtual bool OnLoad(const CString &sArgsi, CString &sMessage);
	virtual bool OnBoot();

	virtual void OnIRCConnected();

	virtual CModule::EModRet OnUserRaw(CString &sLine);
	virtual CModule::EModRet OnUserJoin(CString &sChannel, CString &sKey);
	virtual CModule::EModRet OnPrivMsg(CNick &nick, CString &sMessage);
	virtual CModule::EModRet OnChanMsg(CNick &nick, CChan &channel, CString &sMessage);
	virtual bool OnServerCapAvailable(const CString &sCap);

	private:
	TwitchTMIUpdateTimer *timer;
	std::time_t lastFrankerZ;
};

class TwitchTMIUpdateTimer : public CTimer
{
	friend class TwitchTMI;

	public:
	TwitchTMIUpdateTimer(TwitchTMI *mod);

	private:
	virtual void RunJob();

	private:
	TwitchTMI *mod;
};

class TwitchTMIJob : public CJob
{
	public:
	TwitchTMIJob(TwitchTMI *mod, const CString &channel):mod(mod),channel(channel) {}

	virtual void runThread();
	virtual void runMain();

	private:
	TwitchTMI *mod;
	CString channel;
	CString title;
};

class GenericJob : public CJob
{
	public:
	GenericJob(std::function<void()> threadFunc, std::function<void()> mainFunc):threadFunc(threadFunc),mainFunc(mainFunc) {}

	virtual void runThread() { threadFunc(); }
	virtual void runMain() { mainFunc(); }

	private:
	std::function<void()> threadFunc;
	std::function<void()> mainFunc;
};
