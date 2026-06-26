Scriptname OSFSpikeQuestScript extends Quest
{ [SPIKE] Driver. Attach to OSFSpikeQuest, which needs ONE reference alias (id 0,
  "optional"/fillable). Runs both spikes on quest init; re-run from the console with
  `cqf OSFSpikeQuest RunSpikes`. }

Event OnQuestInit()
	RunSpikes()
EndEvent

Function RunSpikes()
	Debug.Trace("[OSFSpike][A] driver: calling OSFSpike.Ping()")
	bool a = OSFSpike.Ping("hello-from-papyrus")
	Debug.Trace("[OSFSpike][A] driver: Ping() returned " + a)

	Debug.Trace("[OSFSpike][B] driver: calling OSFSpike.RunHandoff(self, player)")
	bool b = OSFSpike.RunHandoff(Self as Quest, Game.GetPlayer())
	Debug.Trace("[OSFSpike][B] driver: RunHandoff() returned " + b)
EndFunction
