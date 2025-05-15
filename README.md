Experimental Data Collection for Targeted Individuals
=====================================================
![EEG Experiment](eeg_experiment.png)

✅ Clean Baseline for Voice Detection

    “Do nothing. Sit still. Keep your eyes open, stare at a fixed point. Let your mind stay empty.”

Why?

    No thinking

    No inner speech

    No physical movement

    No emotional reaction

This makes the EEG signal flat, regular, and stable — so any injected voice causes a clear disturbance.

Assumption: You have Olimex EEG-SMT plugged in and it is working properly when you try it with https://github.com/michaloblastni/local-neural-monitoring

🧪 Repeatable Procedure
Baseline Task:

    Sit in a chair, back straight

    Look at a simple black cross on a white screen or paper

    Eyes open, don’t blink too much

    Don’t think anything. Don’t move. Just observe.

Duration:

    2 minutes before recording starts

    Repeat 1-minute blocks between events

Marking:

    Click "Start Voice Event" to mark a situation when you suddenly hear some insulting comment the task, and click "End Voice Event" to mark when the comment ended. Try to be as precise as possible.

🔍 Why It Works

    You define the "normal EEG" when no voices and no thoughts are present

    Then compare it to the EEG during voice events

    Any sudden activity — spikes, entropy change, gamma burst — is meaningful



📊 Data Analysis
    Compute and compare:

        Band power (especially gamma)

        Entropy

        Sudden shifts
