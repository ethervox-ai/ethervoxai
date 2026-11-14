# Tool Test Scenarios Mapping

This document defines the test scenario for each tool. These scenarios are embedded in the tool definitions and can be used for automated testing.

## Compute Tools

| Tool Name | Test Scenario |
|-----------|---------------|
| `calculator_compute` | Calculate 157 times 43 |
| `percentage_calculate` | What's 15% of 200? |
| `get_time` | What time is it? |
| `get_date` | What's today's date? |
| `get_day` | What day is it? |
| `time_get_week_number` | What week of the year is it? |

## Memory Tools

| Tool Name | Test Scenario |
|-----------|---------------|
| `memory_store` | Remember my favorite color is blue |
| `memory_search` | What's my favorite color? |
| `memory_reminder_list` | What reminders do I have? |
| `memory_complete_reminder` | Mark my grocery reminder as done |
| `memory_update_reminder` | Change my meeting reminder to 5pm |
| `memory_delete` | Delete that reminder |
| `memory_forget` | Forget my birthday |
| `memory_store_correction` | Actually, my name is John, not Jon |
| `memory_store_pattern` | I always drink coffee in the morning |
| `memory_export` | Export all my memories |

## File Tools

| Tool Name | Test Scenario |
|-----------|---------------|
| `file_list` | Show me files in my downloads folder |
| `file_read` | Read the contents of notes.txt |
| `file_search` | Find my resume file |
| `file_write` | Create a file called todo.txt with my tasks |
| `file_append` | Add this line to my todo list |
| `path_list` | What storage locations are available? |
| `path_get` | Show details for the Downloads folder |
| `path_set` | Set working directory to Documents |
| `path_check_unverified` | Check if this path exists |
| `file_set_safe_mode` | Enable safe mode for file operations |

## Conversation Tools

| Tool Name | Test Scenario |
|-----------|---------------|
| `speak` | Say hello out loud |
| `listen` | Listen to my voice note |
| `listen_and_summarize` | Listen to this recording and summarize it |
| `train_pronunciation` | Help me pronounce entrepreneur |

## Unit Conversion

| Tool Name | Test Scenario |
|-----------|---------------|
| `unit_convert` | Convert 10 miles to kilometers |

## System Info Tools

| Tool Name | Test Scenario |
|-----------|---------------|
| `system_version` | What version are you? |
| `system_capabilities` | What can you do? |

## Startup Prompt Tools

| Tool Name | Test Scenario |
|-----------|---------------|
| `startup_prompt_update` | Change your personality to be more formal |
| `startup_prompt_read` | Show me your current system prompt |

## Meta Tools

| Tool Name | Test Scenario |
|-----------|---------------|
| `get_tool_info` | What does the calculator tool do? |

## Timer Tools

| Tool Name | Test Scenario |
|-----------|---------------|
| `timer_set` | Set a timer for 5 minutes |
| `timer_cancel` | Cancel my timer |
| `timer_list` | What timers are running? |

## Weather Tools

| Tool Name | Test Scenario |
|-----------|---------------|
| `weather_current` | What's the weather like right now? |
| `weather_forecast` | What's the weather forecast for tomorrow? |

## Context Tools

| Tool Name | Test Scenario |
|-----------|---------------|
| `context_set_importance` | Set high importance for this topic |
| `context_get_current` | What's the current context? |
| `context_clear` | Clear the current context |

## Voice Tools

| Tool Name | Test Scenario |
|-----------|---------------|
| `voice_set_language` | Change language to Spanish |
| `voice_get_settings` | Show voice settings |

## Workspace Tools

| Tool Name | Test Scenario |
|-----------|---------------|
| `workspace_status` | Show workspace status |
| `workspace_list` | List all workspaces |

## Notes

- Each test scenario is a natural language prompt that should trigger the tool
- Scenarios are designed to be realistic user requests
- Tools can have multiple valid triggers, but one canonical test scenario
- Test scenarios focus on the primary use case for each tool
