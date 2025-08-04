console.log('Starting simple test...');

try {
  console.log('1. Testing core modules...');
  const { multilingualRuntime } = require('./dist/modules/multilingualRuntime');
  console.log('✓ multilingualRuntime loaded');

  const { localLLMStack } = require('./dist/modules/localLLMStack');
  console.log('✓ localLLMStack loaded');

  const { privacyDashboard } = require('./dist/modules/privacyDashboard');
  console.log('✓ privacyDashboard loaded');

  console.log('2. Testing WindowsDesktopDemo class...');
  const { WindowsDesktopDemo } = require('./dist/demo/windows-desktop');
  console.log('✓ WindowsDesktopDemo class loaded');

  console.log('3. Creating demo instance...');
  const demo = new WindowsDesktopDemo();
  console.log('✓ Demo instance created');

  console.log('All tests passed!');
} catch (error) {
  console.error('❌ Test failed:', error.message);
  console.error('Stack:', error.stack);
}
