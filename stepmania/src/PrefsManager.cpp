#include "global.h"
#include "PrefsManager.h"
#include "IniFile.h"
#include "GameState.h"
#include "RageDisplay.h"
#include "RageUtil.h"
#include "arch/arch_default.h"
#include "RageSoundReader_Resample.h" /* for ResampleQuality */
#include "RageFile.h"
#include "ProductInfo.h"
#include "Foreach.h"
#include "Preference.h"
#include "RageLog.h"

#define DEFAULTS_INI_PATH	"Data/Defaults.ini"		// these can be overridden
#define STEPMANIA_INI_PATH	"Data/StepMania.ini"	// overlay on Defaults.ini, contains the user's choices
#define STATIC_INI_PATH		"Data/Static.ini"		// overlay on the 2 above, can't be overridden

PrefsManager*	PREFSMAN = NULL;	// global and accessable from anywhere in our program

const float DEFAULT_SOUND_VOLUME = 1.00f;
const CString DEFAULT_LIGHTS_DRIVER = "Null";

//
// For self-registering prefs
//
#include "SubscriptionManager.h"
template<>
set<IPreference*>* SubscriptionManager<IPreference>::s_pSubscribers = NULL;

void PrefsManager::Subscribe( IPreference *p )
{
	SubscriptionManager<IPreference>::Subscribe( p );
}

void PrefsManager::Unsubscribe( IPreference *p )
{
	SubscriptionManager<IPreference>::Unsubscribe( p );
}

IPreference *PrefsManager::GetPreferenceByName( const CString &sName )
{
	FOREACHS( IPreference*, *SubscriptionManager<IPreference>::s_pSubscribers, p )
	{
		if( !(*p)->GetName().CompareNoCase( sName ) )
			return *p;
	}

	return NULL;
}

bool g_bAutoRestart = false;
#ifdef DEBUG
# define TRUE_IF_DEBUG true
#else
# define TRUE_IF_DEBUG false
#endif

PrefsManager::PrefsManager() :
	m_bWindowed				( "Windowed",				TRUE_IF_DEBUG),
	m_iDisplayWidth			( "DisplayWidth",			640 ),
	m_iDisplayHeight		( "DisplayHeight",			480 ),
	m_iDisplayColorDepth	( "DisplayColorDepth",		16 ),
	m_iTextureColorDepth	( "TextureColorDepth",		16 ),
	m_iMovieColorDepth		( "MovieColorDepth",		16 ),
	m_iMaxTextureResolution	( "MaxTextureResolution",	2048 ),
	m_iRefreshRate			( "RefreshRate",			REFRESH_DEFAULT ),
	m_fDisplayAspectRatio	( "DisplayAspectRatio",		4/3.0f ),
	m_bShowStats			( "ShowStats",				TRUE_IF_DEBUG),
	m_bShowBanners			( "ShowBanners",			true ),

	m_BackgroundMode		( "BackgroundMode",			BGMODE_ANIMATIONS ),
	m_iNumBackgrounds		( "NumBackgrounds",			8 ),
	m_fBGBrightness			( "BGBrightness",			0.8f ),
	/* I'd rather get occasional people asking for support for this even though
     * it's already here than lots of people asking why songs aren't being displayed. */
	m_bHiddenSongs			( "HiddenSongs",			false ),
	m_bVsync				( "Vsync",					true ),
	m_bInterlaced			( "Interlaced",				false ),
	m_bPAL					( "PAL",					false ),
	m_bDelayedTextureDelete	( "DelayedTextureDelete",	true ),
	m_bTexturePreload		( "TexturePreload",			false ),
	m_bDelayedScreenLoad	( "DelayedScreenLoad",		false ),
	m_bDelayedModelDelete	( "DelayedModelDelete",		false ),
	m_BannerCache			( "BannerCache",			BNCACHE_LOW_RES_PRELOAD ),
	m_bPalettedBannerCache	( "PalettedBannerCache",	false ),
	m_bFastLoad				( "FastLoad",				false ),

	m_bOnlyDedicatedMenuButtons	( "OnlyDedicatedMenuButtons",	false ),
	m_bMenuTimer				( "MenuTimer",					true ),
	m_bShowDanger				( "ShowDanger",					true ),

	m_fJudgeWindowScale				( "JudgeWindowScale",				1.0f ),
	m_fJudgeWindowAdd				( "JudgeWindowAdd",					0 ),
	m_fJudgeWindowSecondsTier1	( "JudgeWindowSecondsTier1",	0.0225f ),
	m_fJudgeWindowSecondsTier2	( "JudgeWindowSecondsTier2",		0.045f ),
	m_fJudgeWindowSecondsTier3		( "JudgeWindowSecondsTier3",		0.090f ),
	m_fJudgeWindowSecondsTier4		( "JudgeWindowSecondsTier4",			0.135f ),
	m_fJudgeWindowSecondsTier5		( "JudgeWindowSecondsTier5",			0.180f ),
	m_fJudgeWindowSecondsHeld			( "JudgeWindowSecondsHeld",			0.250f ),	// allow enough time to take foot off and put back on
	m_fJudgeWindowSecondsRoll		( "JudgeWindowSecondsRoll",			0.350f ),
	m_fJudgeWindowSecondsMine		( "JudgeWindowSecondsMine",			0.090f ),	// same as great
	m_fJudgeWindowSecondsAttack		( "JudgeWindowSecondsAttack",		0.135f ),

	m_fLifeDifficultyScale				( "LifeDifficultyScale",				1.0f ),
	m_fLifeDeltaPercentChangeTier1	( "LifeDeltaPercentChangeTier1",	+0.008f ),
	m_fLifeDeltaPercentChangeTier2	( "LifeDeltaPercentChangeTier2",		+0.008f ),
	m_fLifeDeltaPercentChangeTier3		( "LifeDeltaPercentChangeTier3",		+0.004f ),
	m_fLifeDeltaPercentChangeTier4		( "LifeDeltaPercentChangeTier4",			+0.000f ),
	m_fLifeDeltaPercentChangeTier5		( "LifeDeltaPercentChangeTier5",			-0.040f ),
	m_fLifeDeltaPercentChangeMiss		( "LifeDeltaPercentChangeMiss",			-0.080f ),
	m_fLifeDeltaPercentChangeHitMine	( "LifeDeltaPercentChangeHitMine",		-0.160f ),
	m_fLifeDeltaPercentChangeHeld			( "LifeDeltaPercentChangeHeld",			+0.008f ),
	m_fLifeDeltaPercentChangeLetGo			( "LifeDeltaPercentChangeLetGo",			-0.080f ),

	m_fTugMeterPercentChangeTier1	( "TugMeterPercentChangeTier1",		+0.010f ),
	m_fTugMeterPercentChangeTier2		( "TugMeterPercentChangeTier2",		+0.008f ),
	m_fTugMeterPercentChangeTier3		( "TugMeterPercentChangeTier3",			+0.004f ),
	m_fTugMeterPercentChangeTier4		( "TugMeterPercentChangeTier4",			+0.000f ),
	m_fTugMeterPercentChangeTier5			( "TugMeterPercentChangeTier5",			-0.010f ),
	m_fTugMeterPercentChangeMiss		( "TugMeterPercentChangeMiss",			-0.020f ),
	m_fTugMeterPercentChangeHitMine		( "TugMeterPercentChangeHitMine",		-0.040f ),
	m_fTugMeterPercentChangeHeld			( "TugMeterPercentChangeHeld",			+0.008f ),
	m_fTugMeterPercentChangeLetGo			( "TugMeterPercentChangeLetGo",			-0.020f ),

	m_iRegenComboAfterFail			( "RegenComboAfterFail",			10 ),
	m_iRegenComboAfterMiss			( "RegenComboAfterMiss",			5 ),
	m_iMaxRegenComboAfterFail		( "MaxRegenComboAfterFail",			10 ),
	m_iMaxRegenComboAfterMiss		( "MaxRegenComboAfterMiss",			10 ),
	m_bTwoPlayerRecovery			( "TwoPlayerRecovery",				true ),
	m_bMercifulDrain				( "MercifulDrain",					true ),	// negative life deltas are scaled by the players life percentage
	m_bMinimum1FullSongInCourses	( "Minimum1FullSongInCourses",		false ),	// FEoS for 1st song, FailImmediate thereafter
	m_bFailOffInBeginner			( "FailOffInBeginner",				false ),
	m_bFailOffForFirstStageEasy		( "FailOffForFirstStageEasy",		false ),
	m_bMercifulBeginner				( "MercifulBeginner",				false ),

	m_iPercentScoreWeightTier1	( "PercentScoreWeightTier1",	3 ),
	m_iPercentScoreWeightTier2	( "PercentScoreWeightTier2",		2 ),
	m_iPercentScoreWeightTier3		( "PercentScoreWeightTier3",		1 ),
	m_iPercentScoreWeightTier4		( "PercentScoreWeightTier4",			0 ),
	m_iPercentScoreWeightTier5		( "PercentScoreWeightTier5",			0 ),
	m_iPercentScoreWeightMiss		( "PercentScoreWeightMiss",			0 ),
	m_iPercentScoreWeightHitMine	( "PercentScoreWeightHitMine",		-2 ),
	m_iPercentScoreWeightHeld			( "PercentScoreWeightHeld",			3 ),
	m_iPercentScoreWeightLetGo			( "PercentScoreWeightLetGo",			0 ),
	
	m_iGradeWeightTier1		( "GradeWeightTier1",	2 ),
	m_iGradeWeightTier2		( "GradeWeightTier2",		2 ),
	m_iGradeWeightTier3			( "GradeWeightTier3",		1 ),
	m_iGradeWeightTier4			( "GradeWeightTier4",		0 ),
	m_iGradeWeightTier5			( "GradeWeightTier5",			-4 ),
	m_iGradeWeightMiss			( "GradeWeightMiss",		-8 ),
	m_iGradeWeightHitMine		( "GradeWeightHitMine",		-8 ),
	m_iGradeWeightHeld			( "GradeWeightHeld",			6 ),
	m_iGradeWeightLetGo			( "GradeWeightLetGo",			0 ),

	m_fSuperMeterPercentChangeTier1	( "SuperMeterPercentChangeTier1",	+0.05f ),
	m_fSuperMeterPercentChangeTier2	( "SuperMeterPercentChangeTier2",		+0.04f ),
	m_fSuperMeterPercentChangeTier3		( "SuperMeterPercentChangeTier3",		+0.02f ),
	m_fSuperMeterPercentChangeTier4		( "SuperMeterPercentChangeTier4",		+0.00f ),
	m_fSuperMeterPercentChangeTier5		( "SuperMeterPercentChangeTier5",			-0.00f ),
	m_fSuperMeterPercentChangeMiss		( "SuperMeterPercentChangeMiss",		-0.20f ),
	m_fSuperMeterPercentChangeHitMine	( "SuperMeterPercentChangeHitMine",		-0.40f ),
	m_fSuperMeterPercentChangeHeld		( "SuperMeterPercentChangeHeld",			+0.04f ),
	m_fSuperMeterPercentChangeLetGo		( "SuperMeterPercentChangeLetGo",			-0.20f ),
	m_bMercifulSuperMeter				( "MercifulSuperMeter",					true ),
	
	m_fTimeMeterSecondsChangeTier1	( "TimeMeterSecondsChangeTier1",	+0.1f ),
	m_fTimeMeterSecondsChangeTier2	( "TimeMeterSecondsChangeTier2",		 0.0f ),
	m_fTimeMeterSecondsChangeTier3		( "TimeMeterSecondsChangeTier3",		-0.5f ),
	m_fTimeMeterSecondsChangeTier4		( "TimeMeterSecondsChangeTier4",			-1.0f ),
	m_fTimeMeterSecondsChangeTier5		( "TimeMeterSecondsChangeTier5",			-2.0f ),
	m_fTimeMeterSecondsChangeMiss		( "TimeMeterSecondsChangeMiss",			-4.0f ),
	m_fTimeMeterSecondsChangeHitMine	( "TimeMeterSecondsChangeHitMine",		-2.0f ),
	m_fTimeMeterSecondsChangeHeld			( "TimeMeterSecondsChangeHeld",			-0.0f ),
	m_fTimeMeterSecondsChangeLetGo			( "TimeMeterSecondsChangeLetGo",			-4.0f ),

	m_AutoPlay					( "AutoPlay",					PC_HUMAN ),
	m_bDelayedBack				( "DelayedBack",				true ),
	m_bShowInstructions			( "ShowInstructions",			true ),
	m_bShowSelectGroup			( "ShowSelectGroup",			true ),
	m_bShowCaution				( "ShowCaution",				true ),
	m_bShowNativeLanguage		( "ShowNativeLanguage",			true ),
	m_bArcadeOptionsNavigation	( "ArcadeOptionsNavigation",	false ),
	m_MusicWheelUsesSections	( "MusicWheelUsesSections",		ALWAYS ),
	m_iMusicWheelSwitchSpeed	( "MusicWheelSwitchSpeed",		10 ),
	m_bEasterEggs				( "EasterEggs",					true ),
	m_Tier1Timing			( "Tier1Timing",			TIER1_EVERYWHERE ),
	m_bEventMode				( "EventMode",					false ),
	m_iCoinsPerCredit			( "CoinsPerCredit",				1 ),
	m_iSongsPerPlay				( "SongsPerPlay",				3 ),

	m_CoinMode						( "CoinMode",					COIN_MODE_HOME ),
	m_Premium						( "Premium",					PREMIUM_NONE ),
	m_bDelayedCreditsReconcile		( "DelayedCreditsReconcile",	false ),
	m_bPickExtraStage				( "PickExtraStage",				false ),

	m_bComboContinuesBetweenSongs	( "ComboContinuesBetweenSongs",	false ),
	m_fLongVerSongSeconds			( "LongVerSongSeconds",			60*2.5f ),	// Dynamite Rave is 2:55
	m_fMarathonVerSongSeconds		( "MarathonVerSongSeconds",		60*5.f ),
	m_ShowSongOptions				( "ShowSongOptions",			YES ),
	m_bSoloSingle					( "SoloSingle",					false ),
	m_bDancePointsForOni			( "DancePointsForOni",			false ),
	m_bPercentageScoring			( "PercentageScoring",			false ),
	m_fMinPercentageForMachineSongHighScore		( "MinPercentageForMachineSongHighScore",	0.5f ),
	m_fMinPercentageForMachineCourseHighScore	( "MinPercentageForMachineCourseHighScore",	0.0001f ),	// don't save course scores with 0 percentage
	m_bDisqualification				( "Disqualification",			false ),
	m_bShowLyrics					( "ShowLyrics",					true ),
	m_bAutogenSteps					( "AutogenSteps",				true ),
	m_bAutogenGroupCourses			( "AutogenGroupCourses",		true ),
	m_bBreakComboToGetItem			( "BreakComboToGetItem",		false ),
	m_bLockCourseDifficulties		( "LockCourseDifficulties",		true ),
	m_ShowDancingCharacters			( "ShowDancingCharacters",		SDC_Random ),
	m_bUseUnlockSystem				( "UseUnlockSystem",			false ),
	m_bAutoMapOnJoyChange			( "AutoMapOnJoyChange",			true ),
	m_fGlobalOffsetSeconds			( "GlobalOffsetSeconds",		0 ),
	m_iProgressiveLifebar			( "ProgressiveLifebar",			0 ),
	m_iProgressiveStageLifebar		( "ProgressiveStageLifebar",	0 ),
	m_iProgressiveNonstopLifebar	( "ProgressiveNonstopLifebar",	0 ),
	m_bShowBeginnerHelper			( "ShowBeginnerHelper",			false ),
	m_bDisableScreenSaver			( "DisableScreenSaver",			true ),
	m_sLanguage						( "Language",					"" ),	// ThemeManager will deal with this invalid language
	m_sMemoryCardProfileSubdir		( "MemoryCardProfileSubdir",	PRODUCT_NAME ),
	m_iProductID					( "ProductID",					1 ),	
	m_sDefaultLocalProfileIDP1		( "DefaultLocalProfileIDP1",	"" ),
	m_sDefaultLocalProfileIDP2		( "DefaultLocalProfileIDP2",	"" ),
	m_bMemoryCards					( "MemoryCards",				false ),
	m_sMemoryCardOsMountPointP1		( "MemoryCardOsMountPointP1",	"" ),
	m_sMemoryCardOsMountPointP2		( "MemoryCardOsMountPointP2",	"" ),
	m_iMemoryCardUsbBusP1			( "MemoryCardUsbBusP1",			-1 ),
	m_iMemoryCardUsbBusP2			( "MemoryCardUsbBusP2",			-1 ),
	m_iMemoryCardUsbPortP1			( "MemoryCardUsbPortP1",		-1 ),
	m_iMemoryCardUsbPortP2			( "MemoryCardUsbPortP2",		-1 ),
	m_iMemoryCardUsbLevelP1			( "MemoryCardUsbLevelP1",		-1 ),
	m_iMemoryCardUsbLevelP2			( "MemoryCardUsbLevelP2",		-1 ),
	m_iCenterImageTranslateX		( "CenterImageTranslateX",		0 ),
	m_iCenterImageTranslateY		( "CenterImageTranslateY",		0 ),
	m_fCenterImageAddWidth			( "CenterImageAddWidth",		0 ),
	m_fCenterImageAddHeight			( "CenterImageAddHeight",		0 ),
	m_fBrightnessAdd				( "BrightnessAdd",				0 ),
	m_AttractSoundFrequency			( "AttractSoundFrequency",		ASF_EVERY_TIME ),
	m_bAllowExtraStage				( "AllowExtraStage",			true ),
	m_bHideDefaultNoteSkin			( "HideDefaultNoteSkin",		false ),
	m_iMaxHighScoresPerListForMachine		( "MaxHighScoresPerListForMachine",		10 ),
	m_iMaxHighScoresPerListForPlayer		( "MaxHighScoresPerListForPlayer",		3 ),
	m_iMaxRecentScoresForMachine			( "MaxRecentScoresForMachine",			100 ),
	m_iMaxRecentScoresForPlayer				( "MaxRecentScoresForPlayer",			20 ),
	m_bAllowMultipleHighScoreWithSameName	( "AllowMultipleHighScoreWithSameName",	true ),
	m_bCelShadeModels						( "CelShadeModels",						false ),	// Work-In-Progress.. disable by default.
	m_bPreferredSortUsesGroups				( "PreferredSortUsesGroups",			true ),

	m_fConstantUpdateDeltaSeconds	( "ConstantUpdateDeltaSeconds",	0 ),
	m_fPadStickSeconds				( "PadStickSeconds",			0 ),
	m_bForceMipMaps					( "ForceMipMaps",				0 ),
	m_bTrilinearFiltering			( "TrilinearFiltering",			0 ),
	m_bAnisotropicFiltering			( "AnisotropicFiltering",		0 ),

	m_bSignProfileData				( "SignProfileData",			false ),
	m_bEditorShowBGChangesPlay		( "EditorShowBGChangesPlay",	false ),
	m_CourseSortOrder				( "CourseSortOrder",			COURSE_SORT_SONGS ),
	m_bMoveRandomToEnd				( "MoveRandomToEnd",			false ),
	m_bSubSortByNumSteps			( "SubSortByNumSteps",			false ),
	m_GetRankingName				( "GetRankingName",				RANKING_ON ),
	m_ScoringType					( "ScoringType",				SCORING_MAX2 ),
	m_BoostAppPriority				( "BoostAppPriority",			BOOST_AUTO ),
	m_sAdditionalSongFolders		( "AdditionalSongFolders",		"" ),
	m_sAdditionalFolders			( "AdditionalFolders",			"" ),
	m_sLastSeenVideoDriver			( "LastSeenVideoDriver",		"" ),
	m_sLastSeenInputDevices			( "LastSeenInputDevices",		"" ),
#if defined(WIN32)
	m_iLastSeenMemory				( "LastSeenMemory",				0 ),
#endif
	m_sVideoRenderers				( "VideoRenderers",				"" ),	// StepMania.cpp sets these on first run:
	m_bSmoothLines					( "SmoothLines",				false ),
	m_sSoundDrivers					( "SoundDrivers",				"" ),
	m_fSoundVolume					( "SoundVolume",				-1 ),	// default
	m_iSoundWriteAhead				( "SoundWriteAhead",			0 ),
	m_iSoundDevice					( "SoundDevice",				"" ),
	m_SoundResampleQuality			( "SoundResampleQuality",		RageSoundReader_Resample::RESAMP_NORMAL ),
	m_sInputDrivers					( "InputDrivers",				"" ),
	m_sLightsDriver					( "LightsDriver",				"" ),
	m_sMovieDrivers					( "MovieDrivers",				"" ),
	m_sLightsStepsDifficulty		( "LightsStepsDifficulty",		"medium" ),
	m_bBlinkGameplayButtonLightsOnNote	( "BlinkGameplayButtonLightsOnNote",false ),
	m_bAllowUnacceleratedRenderer	( "AllowUnacceleratedRenderer",	false ),
	m_bThreadedInput				( "ThreadedInput",				true ),
	m_bThreadedMovieDecode			( "ThreadedMovieDecode",		true ),
	m_bScreenTestMode				( "ScreenTestMode",				false ),
	m_bDebugLights					( "DebugLights",				false ),
	m_bMonkeyInput					( "MonkeyInput",				false ),
	m_sMachineName					( "MachineName",				"" ),
	m_sIgnoredMessageWindows		( "IgnoredMessageWindows",		"" ),
	m_sCoursesToShowRanking			( "CoursesToShowRanking",		"" ),

	/* Debug: */
	m_bLogToDisk					( "LogToDisk",					true ),
	m_bForceLogFlush				( "ForceLogFlush",				false ),
	m_bShowLogOutput				( "ShowLogOutput",				false ),
	m_bLogSkips						( "LogSkips",					false ),
	m_bLogCheckpoints				( "LogCheckpoints",				false ),
	m_bShowLoadingWindow			( "ShowLoadingWindow",			true ),

	/* Game-specific prefs: */
	m_sDefaultModifiers				( "DefaultModifiers",			"" )

#if defined(XBOX)
	,
	m_bEnableVirtualMemory		( "EnableVirtualMemory",			true ),
	m_iPageFileSize				( "PageFileSize",					384 ),
	m_iPageSize					( "PageSize",						16 ),
	m_iPageThreshold			( "PageThreshold",					8 ),
	m_bLogVirtualMemory 		( "LogVirtualMemory",				false )
#endif

#if !defined(WITHOUT_NETWORKING)
	,
	m_bEnableScoreboard			( "EnableScoreboard",				true )
#endif

{
	Init();
	ReadGlobalPrefsFromDisk();
}
#undef TRUE_IF_DEBUG

void PrefsManager::Init()
{
	FOREACHS_CONST( IPreference*, *SubscriptionManager<IPreference>::s_pSubscribers, p )
		(*p)->LoadDefault();
}

PrefsManager::~PrefsManager()
{
}

void PrefsManager::ReadGlobalPrefsFromDisk()
{
	ReadPrefsFromFile( DEFAULTS_INI_PATH );
	ReadPrefsFromFile( STEPMANIA_INI_PATH );
	ReadPrefsFromFile( STATIC_INI_PATH );
}

void PrefsManager::ResetToFactoryDefaults()
{
	// clobber the users prefs by initing then applying defaults
	Init();
	ReadPrefsFromFile( DEFAULTS_INI_PATH );
	ReadPrefsFromFile( STATIC_INI_PATH );
	
	SaveGlobalPrefsToDisk();
}

void PrefsManager::ReadPrefsFromFile( CString sIni )
{
	IniFile ini;
	if( !ini.ReadFile(sIni) )
		return;

	ReadGlobalPrefsFromIni( ini );
}

void PrefsManager::ReadGlobalPrefsFromIni( const IniFile &ini )
{
	FOREACHS_CONST( IPreference*, *SubscriptionManager<IPreference>::s_pSubscribers, p )
		(*p)->ReadFrom( ini );

	// validate
	m_iSongsPerPlay.Set( clamp(m_iSongsPerPlay.Get(),0,MAX_SONGS_PER_PLAY) );
	FOREACH_PlayerNumber( pn )
		GetMemoryCardOsMountPoint(pn).Set( FixSlashes(GetMemoryCardOsMountPoint(pn)) );
	m_BackgroundMode.Set( (BackgroundMode)clamp((int)m_BackgroundMode.Get(),0,(int)NUM_BackgroundMode-1) );
}

void PrefsManager::SaveGlobalPrefsToDisk() const
{
	IniFile ini;
	SaveGlobalPrefsToIni( ini );
	ini.WriteFile( STEPMANIA_INI_PATH );
}

void PrefsManager::SaveGlobalPrefsToIni( IniFile &ini ) const
{
	FOREACHS_CONST( IPreference*, *SubscriptionManager<IPreference>::s_pSubscribers, p )
		(*p)->WriteTo( ini );
}

// wrappers
CString PrefsManager::GetSoundDrivers()	
{
	if ( m_sSoundDrivers.Get().empty() )
		return (CString)DEFAULT_SOUND_DRIVER_LIST;
	else
		return m_sSoundDrivers;
}

float PrefsManager::GetSoundVolume()
{
	if ( m_fSoundVolume==-1 )
		return DEFAULT_SOUND_VOLUME;
	else
		return clamp(m_fSoundVolume.Get(),0.0f,1.0f); 
}

CString PrefsManager::GetInputDrivers()	{
	if( m_sInputDrivers.Get().empty() )
		return (CString)DEFAULT_INPUT_DRIVER_LIST;
	else
		return m_sInputDrivers;
}

CString PrefsManager::GetMovieDrivers()
{ 
	if ( m_sMovieDrivers.Get().empty() )
		return (CString)DEFAULT_MOVIE_DRIVER_LIST;
	else
		return m_sMovieDrivers; 
}

CString PrefsManager::GetLightsDriver()	{ 
	if ( m_sLightsDriver.Get().empty() )
		return (CString)DEFAULT_LIGHTS_DRIVER;
	else
		return m_sLightsDriver;
}


// lua start
#include "LuaBinding.h"

class LunaPrefsManager: public Luna<PrefsManager>
{
public:
	LunaPrefsManager() { LUA->Register( Register ); }

	static int GetPreference( T* p, lua_State *L )
	{
		CString sName = SArg(1);
		IPreference *pPref = PREFSMAN->GetPreferenceByName( sName );
		if( pPref == NULL )
		{
			LOG->Warn( "GetPreference: unknown preference \"%s\"", sName.c_str() );
			lua_pushnil( L );
		}
		else
		{
			pPref->PushValue( L );
		}
		return 1;
	}
	static int SetPreference( T* p, lua_State *L )
	{
		CString sName = SArg(1);

		IPreference *pPref = PREFSMAN->GetPreferenceByName( sName );
		if( pPref == NULL )
		{
			LOG->Warn( "GetPreference: unknown preference \"%s\"", sName.c_str() );
		}
		else
		{
			lua_pushvalue( L, 2 );
			pPref->SetFromStack( L );
		}

		return 0;
	}

	static void Register(lua_State *L)
	{
		ADD_METHOD( GetPreference );
		ADD_METHOD( SetPreference );

		Luna<T>::Register( L );

		// Add global singleton if constructed already.  If it's not constructed yet,
		// then we'll register it later when we reinit Lua just before 
		// initializing the display.
		if( PREFSMAN )
		{
			lua_pushstring(L, "PREFSMAN");
			PREFSMAN->PushSelf( L );
			lua_settable(L, LUA_GLOBALSINDEX);
		}
	}
};

LUA_REGISTER_CLASS( PrefsManager )
// lua end

/*
 * (c) 2001-2004 Chris Danford, Chris Gomez
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
