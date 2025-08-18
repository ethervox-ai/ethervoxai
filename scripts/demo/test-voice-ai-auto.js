#!/usr/bin/env node

/**
 * Quick Test for AI Processing in Voice Demo
 */

const { spawn } = require('child_process');
const path = require('path');

async function testVoiceAI() {
    console.log('🎤 Testing Voice AI Processing');
    console.log('==============================');
    
    return new Promise((resolve, reject) => {
        const child = spawn('node', [
            path.join(__dirname, 'voice-interaction-demo.js')
        ], {
            stdio: ['pipe', 'pipe', 'pipe']
        });
        
        let output = '';
        let isInMenu = false;
        
        child.stdout.on('data', (data) => {
            const text = data.toString();
            output += text;
            console.log(text);
            
            // Wait for the menu to appear
            if (text.includes('> ') && !isInMenu) {
                isInMenu = true;
                console.log('\n📝 Sending option 6 (Debug: Test Custom Text Input)...');
                child.stdin.write('6\n');
            }
            
            // When prompted for text input
            if (text.includes('Enter the text you want to process')) {
                console.log('\n📝 Sending test query: "What time is it?"...');
                child.stdin.write('What time is it?\n');
            }
            
            // When it shows press any key
            if (text.includes('Press any key to return to menu')) {
                console.log('\n📝 Pressing key to continue...');
                child.stdin.write('\n');
                setTimeout(() => {
                    console.log('\n📝 Sending option 8 (Exit)...');
                    child.stdin.write('8\n');
                }, 1000);
            }
        });
        
        child.stderr.on('data', (data) => {
            console.error('Error:', data.toString());
        });
        
        child.on('close', (code) => {
            console.log(`\n✅ Process completed with code ${code}`);
            resolve(output);
        });
        
        // Timeout after 30 seconds
        setTimeout(() => {
            child.kill();
            reject(new Error('Test timeout'));
        }, 30000);
    });
}

testVoiceAI().catch(console.error);
