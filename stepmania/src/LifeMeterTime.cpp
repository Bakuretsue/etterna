#include "global.h"
#include "LifeMeterTime.h"
#include "ThemeManager.h"
#include "Steps.h"
#include "ActorUtil.h"
#include "Course.h"
#include "PrefsManager.h"
#include "StreamDisplay.h"
#include "GameState.h"
#include "StatsManager.h"

const float FULL_LIFE_SECONDS = 1.5f*60;

static ThemeMetric<float> METER_WIDTH		("LifeMeterTime","MeterWidth");
static ThemeMetric<float> METER_HEIGHT		("LifeMeterTime","MeterHeight");
static ThemeMetric<float> DANGER_THRESHOLD	("LifeMeterTime","DangerThreshold");
static ThemeMetric<int> NUM_CHAMBERS		("LifeMeterTime","NumChambers");
static ThemeMetric<int> NUM_STRIPS			("LifeMeterTime","NumStrips");
static ThemeMetric<float> INITIAL_VALUE		("LifeMeterTime","InitialValue");

LifeMeterTime::LifeMeterTime()
{
	m_fLifeTotalGainedSeconds = 0;
	m_fLifeTotalLostSeconds = 0;
}

void LifeMeterTime::Load( const PlayerState *pPlayerState, PlayerStageStats *pPlayerStageStats )
{
	LifeMeter::Load( pPlayerState, pPlayerStageStats );

	const CString sType = "LifeMeterTime";

	m_sprBackground.Load( THEME->GetPathG(sType,"background") );
	m_sprBackground->SetName( "Background" );
	m_sprBackground->ZoomToWidth( METER_WIDTH );
	m_sprBackground->ZoomToHeight( METER_HEIGHT );
	this->AddChild( m_sprBackground );

	m_quadDangerGlow.ZoomToWidth( METER_WIDTH );
	m_quadDangerGlow.ZoomToHeight( METER_HEIGHT );
	m_quadDangerGlow.SetEffectDiffuseShift();
	m_quadDangerGlow.SetEffectColor1( RageColor(1,0,0,0.8f) );
	m_quadDangerGlow.SetEffectColor2( RageColor(1,0,0,0) );
	m_quadDangerGlow.SetEffectClock( Actor::CLOCK_BGM_BEAT );
	this->AddChild( &m_quadDangerGlow );

	m_pStream = new StreamDisplay;
	bool bExtra = GAMESTATE->IsExtraStage()||GAMESTATE->IsExtraStage2();
	CString sExtra = bExtra ? "extra " : "";
	m_pStream->Load(
		METER_WIDTH,
		METER_HEIGHT,
		NUM_STRIPS,
		NUM_CHAMBERS,		
		THEME->GetPathG(sType,sExtra+"normal"),
		THEME->GetPathG(sType,sExtra+"hot"),
		THEME->GetPathG(sType,sExtra+"passing"),
		THEME->GetMetricA(sType,"StreamNormalOnCommand"),
		THEME->GetMetricA(sType,"StreamHotOnCommand"),
		THEME->GetMetricA(sType,"StreamPassingOnCommand")
		);
	this->AddChild( m_pStream );

	m_sprFrame.Load( THEME->GetPathG(sType,sExtra+"frame") );
	m_sprFrame->SetName( "Frame" );
	this->AddChild( m_sprFrame );

	m_soundGainLife.Load( THEME->GetPathS(sType,"GainLife") );
}

void LifeMeterTime::OnLoadSong()
{
	if( m_pPlayerStageStats->bFailedEarlier )
		return;

	Course* pCourse = GAMESTATE->m_pCurCourse;
	ASSERT( pCourse );
	const CourseEntry *pEntry = &pCourse->m_vEntries[GAMESTATE->GetCourseSongIndex()];
	m_fLifeTotalGainedSeconds += pEntry->fGainSeconds;

	if( GAMESTATE->GetCourseSongIndex() > 0 )
		m_soundGainLife.Play();
}


void LifeMeterTime::ChangeLife( TapNoteScore tns )
{
	if( m_pPlayerStageStats->bFailedEarlier )
		return;

	float fMeterChange = 0;
	switch( tns )
	{
	case TNS_Tier1:	fMeterChange = PREFSMAN->m_fTimeMeterSecondsChangeTier1;	break;
	case TNS_Tier2:	fMeterChange = PREFSMAN->m_fTimeMeterSecondsChangeTier2;		break;
	case TNS_Tier3:		fMeterChange = PREFSMAN->m_fTimeMeterSecondsChangeTier3;		break;
	case TNS_Tier4:		fMeterChange = PREFSMAN->m_fTimeMeterSecondsChangeTier4;			break;
	case TNS_Tier5:		fMeterChange = PREFSMAN->m_fTimeMeterSecondsChangeTier5;			break;
	case TNS_Miss:		fMeterChange = PREFSMAN->m_fTimeMeterSecondsChangeMiss;			break;
	case TNS_HitMine:	fMeterChange = PREFSMAN->m_fTimeMeterSecondsChangeHitMine;		break;
	default:	ASSERT(0);
	}

	m_fLifeTotalLostSeconds -= fMeterChange;

	if( GetLifeSeconds() <= 0 )
		m_pPlayerStageStats->bFailedEarlier = true;
}

void LifeMeterTime::ChangeLife( HoldNoteScore hns, TapNoteScore tns )
{
	if( m_pPlayerStageStats->bFailedEarlier )
		return;

	float fMeterChange = 0;
	switch( hns )
	{
	case HNS_Held:	fMeterChange = PREFSMAN->m_fTimeMeterSecondsChangeHeld;	break;
	case HNS_LetGo:	fMeterChange = PREFSMAN->m_fTimeMeterSecondsChangeLetGo;	break;
	default:	ASSERT(0);
	}

	m_fLifeTotalLostSeconds -= fMeterChange;

	if( GetLifeSeconds() <= 0 )
		m_pPlayerStageStats->bFailedEarlier = true;
}

void LifeMeterTime::OnDancePointsChange()
{
}


bool LifeMeterTime::IsInDanger() const
{
	return false;
}

bool LifeMeterTime::IsHot() const
{
	return false;
}

bool LifeMeterTime::IsFailing() const
{
	return m_pPlayerStageStats->bFailedEarlier;
}

void LifeMeterTime::Update( float fDeltaTime )
{
	// update current stage stats so ScoreDisplayLifeTime can show the right thing
	float fSecs = GetLifeSeconds();
	fSecs = max( 0, fSecs );
	m_pPlayerStageStats->fLifeRemainingSeconds = fSecs;
	
	LifeMeter::Update( fDeltaTime );

	// TODO
	m_pStream->SetPercent( GetLife() );
	m_pStream->SetPassingAlpha( 0 );
	m_pStream->SetHotAlpha( 0 );

	if( m_pStream->GetTrailingLifePercent() < DANGER_THRESHOLD && !GAMESTATE->IsPlayerDead(m_pPlayerState) )
		m_quadDangerGlow.SetDiffuseAlpha( 1 );
	else
		m_quadDangerGlow.SetDiffuseAlpha( 0 );
}

float LifeMeterTime::GetLife() const
{
	float fPercent = GetLifeSeconds() / FULL_LIFE_SECONDS;
	CLAMP( fPercent, 0, 1 );
	return fPercent;
}

void LifeMeterTime::ForceFail()
{
	m_fLifeTotalLostSeconds = m_fLifeTotalGainedSeconds + 100;
}

float LifeMeterTime::GetLifeSeconds() const
{
	float fSecs = m_fLifeTotalGainedSeconds - (m_fLifeTotalLostSeconds + STATSMAN->m_CurStageStats.fStepsSeconds);
	return fSecs;
}


/*
 * (c) 2001-2004 Chris Danford
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
