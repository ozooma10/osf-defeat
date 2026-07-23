Scriptname OSFDefeatBasicRobbed extends Quest
{ OSF Defeat - Basic Consequences: "Robbed and Left".

  The SFW default consequence that ships with the framework, and the reference
  consumer for the handoff API. This quest is NOT start-game-enabled: OSF
  Defeat starts it natively when the "Basic: Robbed and Left" consequence JSON
  wins selection, with the down-state already held and auto-claimed under the
  consequence's name (watchdog-protected - if this script dies, the framework
  force-rescues the player; nothing can strand them).

  The beat: your attackers take a cut of your credits and leave you for dead.
  The stolen credits go INTO the aggressor's inventory - hunt them down later
  to get them back. Then you come to, and the fight (if anyone stayed) resumes
  on vanilla terms. }

Float Property DownSeconds = 15.0 Auto Const
{ How long the player stays down after the robbery beat. }

Float Property RobbedPercent = 10.0 Auto Const
{ Cut of the player's credits the aggressor takes (percent). }

Int Property RobbedMin = 25 Auto Const
{ Floor: they always take at least this much (if you have it). }

Int Property RobbedMax = 2500 Auto Const
{ Cap: never lose more than this in one defeat. }

Spell Property HurtAffliction Auto Const
{ An affliction ABILITY added to the player as they go down. Ships filled with
  the vanilla minor sprain (ENV_AFFL_Injury_Sprain_1) - treatable at any doctor
  or with the right aid item, exactly like an environmental injury. Vanilla
  applies afflictions via AddSpell, not Cast (they are ability spells). Clear
  it in xEdit/CK to disable the wound. }

Float Property HurtChance = 0.35 Auto Const
{ Chance (0..1) the beating leaves the affliction. }

Event OnQuestInit()
    RunConsequence()
EndEvent

Function RunConsequence()
    Actor akPlayer    = Game.GetPlayer()
    Actor akAggressor = OSFDefeat.GetLastPlayerAggressor()

    ; --- Robbery: credits move to the aggressor (recoverable by killing them). ---
    If akAggressor != None
        MiscObject kCredits = Game.GetCredits()
        Int iHave = akPlayer.GetItemCount(kCredits)
        Int iTake = ((iHave as Float) * RobbedPercent / 100.0) as Int
        If iTake < RobbedMin
            iTake = RobbedMin
        EndIf
        If iTake > RobbedMax
            iTake = RobbedMax
        EndIf
        If iTake > iHave
            iTake = iHave
        EndIf
        If iTake > 0
            akPlayer.RemoveItem(kCredits, iTake, true, akAggressor)
            Debug.Notification("Robbed while down: " + iTake + " credits taken.")
        EndIf
    EndIf

    ; --- The wound: an affliction ability, the vanilla way (AddSpell). ------
    If HurtAffliction != None && Utility.RandomFloat() < HurtChance
        akPlayer.AddSpell(HurtAffliction, true)
    EndIf

    ; --- The down beat, then hand the down-state back. -----------------------
    ; The framework holds recovery for us (holdRecovery auto-claim). If this
    ; wait outlives the consequence's maxHoldSeconds, the watchdog rescues the
    ; player anyway - by design.
    Utility.Wait(DownSeconds)
    OSFDefeatBasicRecovery.FinishAftermath(akPlayer)

    ; Stop so OSF Defeat can Start us again on the next defeat.
    Self.Stop()
EndFunction
