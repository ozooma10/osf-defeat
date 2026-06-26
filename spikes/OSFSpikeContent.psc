Scriptname OSFSpikeContent Hidden
{ [SPIKE] DispatchStaticCall target - the *content* side that fills its own alias.
  This is the shim that makes the handoff viable, since pure-C++ alias fill is not
  exposed by CommonLibSF. In the real design this lives in the content mod. }

Function Fill(Quest akQuest, Actor akTarget) global
	ReferenceAlias al = akQuest.GetAlias(0) as ReferenceAlias  ; alias id 0 (verify in CK/xEdit)
	If al
		al.ForceRefTo(akTarget)
		Debug.Trace("[OSFSpike][B] content Fill: alias0 -> " + al.GetReference() + " (wanted " + akTarget + ")")
	Else
		Debug.Trace("[OSFSpike][B] content Fill: quest alias id 0 missing or not a ReferenceAlias")
	EndIf
EndFunction
