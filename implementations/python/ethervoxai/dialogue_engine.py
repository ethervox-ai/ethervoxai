"""
🧠 EthervoxAI Dialogue Engine

Handles intent parsing, conversation management, and LLM integration
for natural language understanding and response generation.
"""

import asyncio
import logging
import json
import re
import time
from typing import Dict, List, Optional, Any, Callable
from dataclasses import dataclass, asdict
from enum import Enum
import sqlite3
from pathlib import Path

try:
    from transformers import pipeline, AutoTokenizer, AutoModelForCausalLM
    import torch
    from sentence_transformers import SentenceTransformer
    ML_DEPENDENCIES_AVAILABLE = True
except ImportError:
    ML_DEPENDENCIES_AVAILABLE = False
    logging.warning("ML dependencies not available - using rule-based fallbacks")

logger = logging.getLogger(__name__)

class IntentType(Enum):
    """Supported intent types"""
    GREETING = "greeting"
    QUESTION = "question"
    COMMAND = "command"
    INFORMATION_REQUEST = "information_request"
    DEVICE_CONTROL = "device_control"
    WEATHER = "weather"
    TIME = "time"
    TIMER = "timer"
    REMINDER = "reminder"
    MUSIC = "music"
    NEWS = "news"
    CALCULATION = "calculation"
    UNKNOWN = "unknown"

class ResponseType(Enum):
    """Response types"""
    TEXT = "text"
    ACTION = "action"
    QUESTION = "question"
    ERROR = "error"

@dataclass
class Intent:
    """Represents a parsed user intent"""
    type: IntentType
    confidence: float
    entities: Dict[str, Any]
    original_text: str
    language: str = "en"
    timestamp: float = 0.0

@dataclass
class DialogueResponse:
    """Response from the dialogue engine"""
    text: str
    type: ResponseType
    intent: Optional[Intent] = None
    actions: List[Dict[str, Any]] = None
    metadata: Dict[str, Any] = None
    confidence: float = 1.0

@dataclass
class ConversationContext:
    """Maintains conversation context"""
    user_id: str
    session_id: str
    language: str = "en"
    history: List[Dict[str, Any]] = None
    user_preferences: Dict[str, Any] = None
    current_topic: Optional[str] = None
    last_intent: Optional[Intent] = None

class IntentClassifier:
    """Classifies user intents using ML and rule-based approaches"""
    
    def __init__(self):
        self.model = None
        self.embeddings_model = None
        self._init_models()
        self._init_intent_patterns()
    
    def _init_models(self):
        """Initialize ML models if available"""
        if not ML_DEPENDENCIES_AVAILABLE:
            logger.info("Using rule-based intent classification")
            return
        
        try:
            # Load sentence transformer for intent classification
            self.embeddings_model = SentenceTransformer('all-MiniLM-L6-v2')
            logger.info("Intent classification models loaded successfully")
        except Exception as e:
            logger.warning(f"Could not load ML models: {e}")
    
    def _init_intent_patterns(self):
        """Initialize rule-based intent patterns"""
        self.intent_patterns = {
            IntentType.GREETING: [
                r'\b(hello|hi|hey|good morning|good afternoon|good evening)\b',
                r'\b(how are you|what\'s up|how\'s it going)\b'
            ],
            IntentType.QUESTION: [
                r'\b(what|who|when|where|why|how|which)\b.*\?',
                r'^(can you|could you|would you|will you)',
                r'\b(tell me|explain|describe)\b'
            ],
            IntentType.COMMAND: [
                r'^(please |)(turn on|turn off|start|stop|open|close|play|pause)',
                r'^(set|change|adjust|modify)',
                r'^(go to|navigate to|switch to)'
            ],
            IntentType.TIME: [
                r'\b(what time|current time|time is it)\b',
                r'\b(what\'s the time|tell me the time)\b'
            ],
            IntentType.WEATHER: [
                r'\b(weather|temperature|forecast|rain|sunny|cloudy)\b',
                r'\b(how hot|how cold|degrees)\b'
            ],
            IntentType.TIMER: [
                r'\b(set timer|start timer|timer for)\b',
                r'\b(countdown|alarm|remind me in)\b'
            ],
            IntentType.CALCULATION: [
                r'\b(calculate|compute|what is|what\'s)\b.*\b(\+|\-|\*|\/|plus|minus|times|divided)\b',
                r'\b(\d+\s*(plus|minus|times|divided by)\s*\d+)\b'
            ],
            IntentType.MUSIC: [
                r'\b(play music|play song|music|spotify|youtube)\b',
                r'\b(next song|previous song|volume)\b'
            ]
        }
    
    async def classify_intent(self, text: str, language: str = "en") -> Intent:
        """Classify user intent from text"""
        text_lower = text.lower().strip()
        
        # Try ML-based classification first
        if self.embeddings_model is not None:
            intent = await self._classify_with_ml(text, language)
            if intent.confidence > 0.7:
                return intent
        
        # Fallback to rule-based classification
        return self._classify_with_rules(text, language)
    
    async def _classify_with_ml(self, text: str, language: str) -> Intent:
        """ML-based intent classification"""
        try:
            # This is a simplified example - in real implementation,
            # you would train a proper intent classification model
            embeddings = self.embeddings_model.encode([text])
            
            # For demo, return a generic intent
            return Intent(
                type=IntentType.QUESTION,
                confidence=0.8,
                entities={},
                original_text=text,
                language=language,
                timestamp=time.time()
            )
        except Exception as e:
            logger.error(f"ML intent classification error: {e}")
            return Intent(
                type=IntentType.UNKNOWN,
                confidence=0.0,
                entities={},
                original_text=text,
                language=language,
                timestamp=time.time()
            )
    
    def _classify_with_rules(self, text: str, language: str) -> Intent:
        """Rule-based intent classification"""
        text_lower = text.lower().strip()
        
        for intent_type, patterns in self.intent_patterns.items():
            for pattern in patterns:
                if re.search(pattern, text_lower):
                    entities = self._extract_entities(text, intent_type)
                    return Intent(
                        type=intent_type,
                        confidence=0.9,
                        entities=entities,
                        original_text=text,
                        language=language,
                        timestamp=time.time()
                    )
        
        return Intent(
            type=IntentType.UNKNOWN,
            confidence=0.1,
            entities={},
            original_text=text,
            language=language,
            timestamp=time.time()
        )
    
    def _extract_entities(self, text: str, intent_type: IntentType) -> Dict[str, Any]:
        """Extract entities based on intent type"""
        entities = {}
        text_lower = text.lower()
        
        if intent_type == IntentType.TIMER:
            # Extract time duration
            time_match = re.search(r'(\d+)\s*(second|minute|hour|min|sec|hr)s?', text_lower)
            if time_match:
                entities['duration'] = {
                    'value': int(time_match.group(1)),
                    'unit': time_match.group(2)
                }
        
        elif intent_type == IntentType.WEATHER:
            # Extract location
            location_match = re.search(r'\bin\s+([a-zA-Z\s]+)', text)
            if location_match:
                entities['location'] = location_match.group(1).strip()
        
        elif intent_type == IntentType.MUSIC:
            # Extract song/artist
            play_match = re.search(r'play\s+(.+)', text_lower)
            if play_match:
                entities['query'] = play_match.group(1).strip()
        
        return entities

class ResponseGenerator:
    """Generates responses using LLM and templates"""
    
    def __init__(self):
        self.llm_model = None
        self.tokenizer = None
        self._init_llm()
        self._init_response_templates()
    
    def _init_llm(self):
        """Initialize local LLM if available"""
        if not ML_DEPENDENCIES_AVAILABLE:
            logger.info("Using template-based response generation")
            return
        
        try:
            # Try to load a small local model for response generation
            # In production, you might use a quantized model or API
            model_name = "microsoft/DialoGPT-small"
            self.tokenizer = AutoTokenizer.from_pretrained(model_name)
            self.llm_model = AutoModelForCausalLM.from_pretrained(model_name)
            logger.info(f"LLM model loaded: {model_name}")
        except Exception as e:
            logger.warning(f"Could not load LLM model: {e}")
    
    def _init_response_templates(self):
        """Initialize response templates for different intents"""
        self.response_templates = {
            IntentType.GREETING: [
                "Hello! How can I help you today?",
                "Hi there! What can I do for you?",
                "Good to see you! How may I assist you?"
            ],
            IntentType.UNKNOWN: [
                "I'm not sure I understand. Could you please rephrase that?",
                "I didn't quite catch that. Can you try asking differently?",
                "Sorry, I'm not sure what you mean. Could you be more specific?"
            ],
            IntentType.TIME: [
                "The current time is {time}.",
                "Right now it's {time}."
            ],
            IntentType.WEATHER: [
                "I'd need to check the weather service for that information.",
                "Weather information isn't available right now, but I can help with other things."
            ],
            IntentType.TIMER: [
                "I'll set a timer for {duration}.",
                "Timer set for {duration}."
            ]
        }
    
    async def generate_response(self, intent: Intent, context: ConversationContext) -> DialogueResponse:
        """Generate response for given intent and context"""
        
        # Try LLM-based generation first
        if self.llm_model is not None:
            response = await self._generate_with_llm(intent, context)
            if response.confidence > 0.7:
                return response
        
        # Fallback to template-based generation
        return self._generate_with_templates(intent, context)
    
    async def _generate_with_llm(self, intent: Intent, context: ConversationContext) -> DialogueResponse:
        """Generate response using LLM"""
        try:
            # Prepare prompt for the LLM
            prompt = f"User: {intent.original_text}\nAssistant:"
            
            # Tokenize and generate
            inputs = self.tokenizer.encode(prompt, return_tensors='pt')
            
            with torch.no_grad():
                outputs = self.llm_model.generate(
                    inputs,
                    max_length=inputs.shape[1] + 50,
                    num_return_sequences=1,
                    temperature=0.7,
                    pad_token_id=self.tokenizer.eos_token_id
                )
            
            response_text = self.tokenizer.decode(outputs[0], skip_special_tokens=True)
            # Extract just the assistant's response
            response_text = response_text.split("Assistant:")[-1].strip()
            
            return DialogueResponse(
                text=response_text,
                type=ResponseType.TEXT,
                intent=intent,
                confidence=0.8
            )
        except Exception as e:
            logger.error(f"LLM generation error: {e}")
            return DialogueResponse(
                text="I'm having trouble processing that right now.",
                type=ResponseType.ERROR,
                intent=intent,
                confidence=0.1
            )
    
    def _generate_with_templates(self, intent: Intent, context: ConversationContext) -> DialogueResponse:
        """Generate response using templates"""
        templates = self.response_templates.get(intent.type, self.response_templates[IntentType.UNKNOWN])
        
        # Select template (simple round-robin for now)
        template = templates[0]
        
        # Fill in template variables
        response_text = self._fill_template(template, intent, context)
        
        # Determine response type and actions
        response_type = ResponseType.TEXT
        actions = []
        
        if intent.type == IntentType.TIMER:
            actions.append({
                'type': 'set_timer',
                'duration': intent.entities.get('duration', {})
            })
        elif intent.type == IntentType.COMMAND:
            response_type = ResponseType.ACTION
            actions.append({
                'type': 'device_control',
                'command': intent.original_text
            })
        
        return DialogueResponse(
            text=response_text,
            type=response_type,
            intent=intent,
            actions=actions,
            confidence=0.9
        )
    
    def _fill_template(self, template: str, intent: Intent, context: ConversationContext) -> str:
        """Fill template with actual values"""
        filled_template = template
        
        # Fill common variables
        if '{time}' in template:
            import datetime
            current_time = datetime.datetime.now().strftime("%I:%M %p")
            filled_template = filled_template.replace('{time}', current_time)
        
        if '{duration}' in template and 'duration' in intent.entities:
            duration = intent.entities['duration']
            duration_str = f"{duration['value']} {duration['unit']}{'s' if duration['value'] > 1 else ''}"
            filled_template = filled_template.replace('{duration}', duration_str)
        
        return filled_template

class ConversationManager:
    """Manages conversation state and history"""
    
    def __init__(self, db_path: str = None):
        self.db_path = db_path or "ethervoxai_conversations.db"
        self.contexts: Dict[str, ConversationContext] = {}
        self._init_database()
    
    def _init_database(self):
        """Initialize SQLite database for conversation history"""
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()
            
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS conversations (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    user_id TEXT NOT NULL,
                    session_id TEXT NOT NULL,
                    timestamp REAL NOT NULL,
                    intent_type TEXT,
                    user_text TEXT,
                    response_text TEXT,
                    language TEXT DEFAULT 'en'
                )
            ''')
            
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS user_preferences (
                    user_id TEXT PRIMARY KEY,
                    preferences TEXT,
                    updated_at REAL NOT NULL
                )
            ''')
            
            conn.commit()
            conn.close()
            logger.info(f"Conversation database initialized: {self.db_path}")
        except Exception as e:
            logger.error(f"Database initialization error: {e}")
    
    def get_context(self, user_id: str, session_id: str = None) -> ConversationContext:
        """Get or create conversation context"""
        if session_id is None:
            session_id = f"session_{int(time.time())}"
        
        context_key = f"{user_id}_{session_id}"
        
        if context_key not in self.contexts:
            self.contexts[context_key] = ConversationContext(
                user_id=user_id,
                session_id=session_id,
                history=[],
                user_preferences=self._load_user_preferences(user_id)
            )
        
        return self.contexts[context_key]
    
    def _load_user_preferences(self, user_id: str) -> Dict[str, Any]:
        """Load user preferences from database"""
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()
            
            cursor.execute(
                "SELECT preferences FROM user_preferences WHERE user_id = ?",
                (user_id,)
            )
            result = cursor.fetchone()
            conn.close()
            
            if result:
                return json.loads(result[0])
            else:
                return {}
        except Exception as e:
            logger.error(f"Error loading user preferences: {e}")
            return {}
    
    def save_interaction(self, context: ConversationContext, intent: Intent, response: DialogueResponse):
        """Save interaction to database and update context"""
        try:
            # Add to context history
            interaction = {
                'timestamp': time.time(),
                'intent': asdict(intent),
                'response': asdict(response)
            }
            context.history.append(interaction)
            context.last_intent = intent
            
            # Save to database
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()
            
            cursor.execute('''
                INSERT INTO conversations 
                (user_id, session_id, timestamp, intent_type, user_text, response_text, language)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            ''', (
                context.user_id,
                context.session_id,
                interaction['timestamp'],
                intent.type.value,
                intent.original_text,
                response.text,
                intent.language
            ))
            
            conn.commit()
            conn.close()
        except Exception as e:
            logger.error(f"Error saving interaction: {e}")

class DialogueEngine:
    """Main dialogue engine coordinator"""
    
    def __init__(self, db_path: str = None):
        self.intent_classifier = IntentClassifier()
        self.response_generator = ResponseGenerator()
        self.conversation_manager = ConversationManager(db_path)
        logger.info("Dialogue engine initialized")
    
    async def process_input(self, text: str, user_id: str = "default", 
                          session_id: str = None, language: str = "en") -> DialogueResponse:
        """Process user input and generate response"""
        
        # Get conversation context
        context = self.conversation_manager.get_context(user_id, session_id)
        context.language = language
        
        # Classify intent
        intent = await self.intent_classifier.classify_intent(text, language)
        logger.info(f"Classified intent: {intent.type.value} (confidence: {intent.confidence})")
        
        # Generate response
        response = await self.response_generator.generate_response(intent, context)
        logger.info(f"Generated response: {response.text}")
        
        # Save interaction
        self.conversation_manager.save_interaction(context, intent, response)
        
        return response
    
    def get_conversation_history(self, user_id: str, session_id: str = None) -> List[Dict[str, Any]]:
        """Get conversation history for user/session"""
        context = self.conversation_manager.get_context(user_id, session_id)
        return context.history
    
    async def process_voice_command(self, voice_command, user_id: str = "default") -> DialogueResponse:
        """Process voice command from voice processor"""
        return await self.process_input(
            voice_command.text,
            user_id=user_id,
            language=voice_command.language
        )

# Convenience function
async def create_dialogue_engine(db_path: str = None) -> DialogueEngine:
    """Create and return a dialogue engine"""
    return DialogueEngine(db_path)
