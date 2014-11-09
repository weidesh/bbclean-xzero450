#pragma once
#include <windows.h>
#include "worker.h"
#include "history.h"
#include "index.h"
#include "jobmanager.h"
namespace bb { namespace search {

struct ProgramLookup;
ProgramLookup & getLookup();
void startLookup (tstring const & path);
void stopLookup ();

struct RebuildJob : Runnable
{
	bb::search::Index & m_index;
	std::atomic<bool> m_finished;

	RebuildJob (bb::search::Index & i) : m_index(i), m_finished(false) { }
	virtual void Run ()
	{
		m_index.Rebuild();
		m_finished.store(true, std::memory_order_relaxed);
	}
	bool IsFinished ()
	{
		return m_finished.load(std::memory_order_relaxed);
	}
};

struct ProgramLookup
{
	tstring const & m_path;
	History m_history;
	Index m_index;
	JobManager m_jobs;

	ProgramLookup (tstring const & path) 
		: m_path(path)
		, m_history(path, TEXT("programs_history"))
		, m_index(path, TEXT("programs"))
	{ }

	void Stop ()
	{
		m_jobs.Stop(); // calls Reset also
	}

	bool Load ()
	{
		m_history.Load();
		return m_index.Load();
	}

	bool IsLoaded () const { return m_index.IsLoaded(); }

	bool IsIndexing () const
	{
		return m_jobs.m_readyQueueLock.load(std::memory_order_relaxed);
	}

	bool LoadOrBuild (bool sync = false)
	{
		m_history.Load();
		if (!m_index.Load())
		{
			m_jobs.Create(1);
			RebuildJob job(m_index);
			m_jobs.AddJob(&job);
			if (sync)
			{
				unsigned c = 0;
				while (!job.IsFinished())
					delayExecution(c);
			}
			return false;
		}

		return true;
	}

	bool Save ()
	{
		if (m_history.Save())
			if (m_index.Save())
				return true;
		return false;
	}

	bool Find (tstring const & what, tstrings & hkeys, tstrings & hres, tstrings & ikeys, tstrings & ires)
	{
		std::vector<HistoryItem *> history_res; //@TODO: unlocal
		history_res.reserve(32);
		if (m_history.Find(what, history_res, 8))
		{
			for (HistoryItem const * h : history_res)
				hkeys.push_back(h->m_fname);
			for (HistoryItem const * h : history_res)
				hres.push_back(h->m_fpath);
			return true;
		}
		else
		{
			if (m_index.Suggest(what, ikeys, ires, 64))
			{
				return true;
			}
		}
		return false;
	}
};

}}


