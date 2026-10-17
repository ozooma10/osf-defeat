Scriptname OSFDefeatAcceptanceProbe Hidden Native
Function Record(String name, Bool ok, Actor subject = None, Float value = 0.0) global native
Bool Function VerifyDeathState(Actor subject, Bool papyrusDead, Bool expectedDead, String checkpoint) global native
Bool Function VerifyWeapon(Actor owner, Form weapon, String checkpoint) global native
Bool Function VerifyDroppedWeapon(ObjectReference reference, Form weapon) global native
Bool Function HoldBlackout(Int checkpoint) global native
Bool Function ApplyScriptedDamage(Actor target, Actor source, Int kind) global native
