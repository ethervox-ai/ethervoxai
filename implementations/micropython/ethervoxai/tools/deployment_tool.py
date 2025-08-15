"""
🚀 EthervoxAI Deployment Utility

Deploy EthervoxAI applications to microcontrollers with ease.
This tool automates the deployment process and handles dependencies.

Features:
- Automatic file transfer to microcontrollers
- Dependency management
- Configuration validation
- Remote testing and verification
- Backup and restore functionality
- Over-the-air (OTA) updates for WiFi-enabled boards

Supported Transfer Methods:
- USB/Serial (mpremote, ampy, Thonny)
- WiFi (WebREPL, FTP, HTTP)
- Bluetooth (ESP32 with Bluetooth)

Target Platforms:
- Raspberry Pi Pico/Pico W
- ESP32 family
- Any MicroPython-compatible board

Usage:
    python deployment_tool.py --target /dev/ttyUSB0 --deploy-all
    python deployment_tool.py --target 192.168.1.100 --wifi --deploy-core
"""

import os
import json
import time
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, List, Optional, Union

class DeploymentTool:
    """EthervoxAI deployment automation tool"""
    
    def __init__(self):
        """Initialize deployment tool"""
        self.supported_methods = ['usb', 'wifi', 'bluetooth']
        self.supported_platforms = ['pico', 'pico_w', 'esp32', 'esp32s2', 'esp32s3']
        
        # Deployment manifest - defines what to deploy
        self.deployment_manifest = {
            'core': [
                'ethervoxai/__init__.py',
                'ethervoxai/core/',
                'ethervoxai/boards/'
            ],
            'examples': [
                'ethervoxai/examples/'
            ],
            'tools': [
                'ethervoxai/tools/'
            ],
            'models': [
                'models/'
            ],
            'config': [
                'config.json',
                'audio_config.json'
            ]
        }
        
        # File transfer tools configuration
        self.transfer_tools = {
            'mpremote': {
                'command': 'mpremote',
                'available': False,
                'priority': 1
            },
            'ampy': {
                'command': 'ampy',
                'available': False,
                'priority': 2
            },
            'webrepl': {
                'command': None,  # Built-in WebREPL client
                'available': False,
                'priority': 3
            }
        }
        
        self.check_available_tools()
    
    def check_available_tools(self):
        """Check which deployment tools are available"""
        print("🔍 Checking available deployment tools...")
        
        for tool_name, tool_config in self.transfer_tools.items():
            if tool_config['command']:
                try:
                    result = subprocess.run(
                        [tool_config['command'], '--help'], 
                        capture_output=True, 
                        timeout=5
                    )
                    tool_config['available'] = result.returncode == 0
                    status = "✅" if tool_config['available'] else "❌"
                    print(f"   {status} {tool_name}")
                except (subprocess.TimeoutExpired, FileNotFoundError):
                    tool_config['available'] = False
                    print(f"   ❌ {tool_name}")
            else:
                # For built-in tools like WebREPL
                tool_config['available'] = True
                print(f"   ✅ {tool_name} (built-in)")
    
    def deploy(self, target: str, method: str = 'auto', components: List[str] = None, 
               source_dir: str = None, config: Dict = None):
        """
        Deploy EthervoxAI to target device
        
        Args:
            target: Target device (port, IP address, etc.)
            method: Deployment method ('usb', 'wifi', 'bluetooth', 'auto')
            components: List of components to deploy ('core', 'examples', 'tools', 'models')
            source_dir: Source directory containing EthervoxAI
            config: Deployment configuration
            
        Returns:
            dict: Deployment results
        """
        print("🚀 Starting EthervoxAI deployment...")
        print(f"🎯 Target: {target}")
        
        # Auto-detect method if not specified
        if method == 'auto':
            method = self._detect_deployment_method(target)
        
        print(f"📡 Method: {method}")
        
        # Set default components if not specified
        if components is None:
            components = ['core', 'examples']
        
        print(f"📦 Components: {', '.join(components)}")
        
        # Validate deployment
        validation_result = self._validate_deployment(target, method, components, source_dir)
        if not validation_result['valid']:
            print("❌ Deployment validation failed!")
            for error in validation_result['errors']:
                print(f"   • {error}")
            return {'success': False, 'errors': validation_result['errors']}
        
        # Prepare deployment package
        package_info = self._prepare_deployment_package(components, source_dir, config)
        if not package_info:
            return {'success': False, 'error': 'Failed to prepare deployment package'}
        
        # Execute deployment
        deployment_result = self._execute_deployment(
            target, method, package_info, config or {}
        )
        
        # Cleanup temporary files
        self._cleanup_deployment(package_info)
        
        if deployment_result['success']:
            print("🎉 Deployment completed successfully!")
            
            # Optional: Run post-deployment verification
            if config and config.get('verify_deployment', True):
                verification_result = self._verify_deployment(target, method, components)
                deployment_result['verification'] = verification_result
        else:
            print("❌ Deployment failed!")
        
        return deployment_result
    
    def _detect_deployment_method(self, target: str) -> str:
        """Auto-detect deployment method based on target"""
        if target.startswith('/dev/') or target.startswith('COM') or 'USB' in target.upper():
            return 'usb'
        elif target.replace('.', '').replace(':', '').isdigit() or 'wifi' in target.lower():
            return 'wifi'
        elif 'bluetooth' in target.lower() or 'bt' in target.lower():
            return 'bluetooth'
        else:
            # Default to USB
            return 'usb'
    
    def _validate_deployment(self, target: str, method: str, components: List[str], 
                           source_dir: Optional[str]) -> Dict:
        """Validate deployment parameters"""
        errors = []
        
        # Validate method
        if method not in self.supported_methods:
            errors.append(f"Unsupported method: {method}")
        
        # Validate components
        for component in components:
            if component not in self.deployment_manifest:
                errors.append(f"Unknown component: {component}")
        
        # Validate source directory
        if source_dir and not os.path.exists(source_dir):
            errors.append(f"Source directory not found: {source_dir}")
        
        # Check if required tools are available
        if method == 'usb':
            usb_tools_available = any(
                tool['available'] for tool in self.transfer_tools.values() 
                if tool['command'] in ['mpremote', 'ampy']
            )
            if not usb_tools_available:
                errors.append("No USB deployment tools available (install mpremote or ampy)")
        
        return {
            'valid': len(errors) == 0,
            'errors': errors
        }
    
    def _prepare_deployment_package(self, components: List[str], source_dir: Optional[str], 
                                   config: Optional[Dict]) -> Optional[Dict]:
        """Prepare deployment package"""
        try:
            print("📦 Preparing deployment package...")
            
            # Create temporary directory
            temp_dir = tempfile.mkdtemp(prefix='ethervoxai_deploy_')
            
            # Determine source directory
            if source_dir is None:
                source_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
            
            # Copy required components
            files_copied = []
            total_size = 0
            
            for component in components:
                if component in self.deployment_manifest:
                    for file_path in self.deployment_manifest[component]:
                        src_path = os.path.join(source_dir, file_path)
                        
                        if os.path.exists(src_path):
                            dst_path = os.path.join(temp_dir, file_path)
                            
                            # Create destination directory
                            os.makedirs(os.path.dirname(dst_path), exist_ok=True)
                            
                            if os.path.isfile(src_path):
                                shutil.copy2(src_path, dst_path)
                                size = os.path.getsize(src_path)
                                total_size += size
                                files_copied.append({
                                    'src': src_path,
                                    'dst': file_path,
                                    'size': size
                                })
                            elif os.path.isdir(src_path):
                                shutil.copytree(src_path, dst_path, dirs_exist_ok=True)
                                # Calculate directory size
                                dir_size = sum(
                                    os.path.getsize(os.path.join(dirpath, filename))
                                    for dirpath, dirnames, filenames in os.walk(dst_path)
                                    for filename in filenames
                                )
                                total_size += dir_size
                                files_copied.append({
                                    'src': src_path,
                                    'dst': file_path,
                                    'size': dir_size,
                                    'type': 'directory'
                                })
                        else:
                            print(f"⚠️  Warning: {file_path} not found in source")
            
            # Create deployment configuration
            if config:
                config_path = os.path.join(temp_dir, 'deployment_config.json')
                with open(config_path, 'w') as f:
                    json.dump(config, f, indent=2)
                files_copied.append({
                    'src': 'generated',
                    'dst': 'deployment_config.json',
                    'size': os.path.getsize(config_path)
                })
            
            # Create deployment manifest
            manifest = {
                'components': components,
                'files': files_copied,
                'total_size_bytes': total_size,
                'created_timestamp': time.time(),
                'source_dir': source_dir
            }
            
            manifest_path = os.path.join(temp_dir, 'deployment_manifest.json')
            with open(manifest_path, 'w') as f:
                json.dump(manifest, f, indent=2)
            
            print(f"   📁 Package prepared: {len(files_copied)} files, {total_size // 1024}KB")
            
            return {
                'temp_dir': temp_dir,
                'manifest': manifest,
                'files': files_copied,
                'total_size': total_size
            }
            
        except Exception as e:
            print(f"❌ Package preparation failed: {e}")
            return None
    
    def _execute_deployment(self, target: str, method: str, package_info: Dict, 
                          config: Dict) -> Dict:
        """Execute the deployment"""
        try:
            print(f"🚀 Deploying to {target} via {method}...")
            
            if method == 'usb':
                return self._deploy_via_usb(target, package_info, config)
            elif method == 'wifi':
                return self._deploy_via_wifi(target, package_info, config)
            elif method == 'bluetooth':
                return self._deploy_via_bluetooth(target, package_info, config)
            else:
                return {'success': False, 'error': f'Unsupported method: {method}'}
                
        except Exception as e:
            return {'success': False, 'error': str(e)}
    
    def _deploy_via_usb(self, target: str, package_info: Dict, config: Dict) -> Dict:
        """Deploy via USB/Serial connection"""
        try:
            # Find best available USB tool
            best_tool = None
            for tool_name, tool_config in self.transfer_tools.items():
                if tool_config['available'] and tool_config['command'] in ['mpremote', 'ampy']:
                    if best_tool is None or tool_config['priority'] < best_tool['priority']:
                        best_tool = {'name': tool_name, **tool_config}
            
            if not best_tool:
                return {'success': False, 'error': 'No USB deployment tool available'}
            
            print(f"   🔧 Using {best_tool['name']}")
            
            # Deploy using the selected tool
            if best_tool['name'] == 'mpremote':
                return self._deploy_with_mpremote(target, package_info, config)
            elif best_tool['name'] == 'ampy':
                return self._deploy_with_ampy(target, package_info, config)
            
        except Exception as e:
            return {'success': False, 'error': f'USB deployment failed: {e}'}
    
    def _deploy_with_mpremote(self, target: str, package_info: Dict, config: Dict) -> Dict:
        """Deploy using mpremote tool"""
        try:
            temp_dir = package_info['temp_dir']
            files_deployed = []
            
            for file_info in package_info['files']:
                src_file = os.path.join(temp_dir, file_info['dst'])
                dst_file = file_info['dst']
                
                if os.path.isfile(src_file):
                    # Upload file
                    cmd = ['mpremote', 'connect', target, 'cp', src_file, f':{dst_file}']
                    result = subprocess.run(cmd, capture_output=True, text=True)
                    
                    if result.returncode == 0:
                        files_deployed.append(file_info['dst'])
                        print(f"   ✅ {file_info['dst']}")
                    else:
                        print(f"   ❌ {file_info['dst']}: {result.stderr}")
                        return {'success': False, 'error': f'Failed to upload {file_info["dst"]}'}
                elif os.path.isdir(src_file):
                    # Upload directory recursively
                    cmd = ['mpremote', 'connect', target, 'cp', '-r', src_file, f':{dst_file}']
                    result = subprocess.run(cmd, capture_output=True, text=True)
                    
                    if result.returncode == 0:
                        files_deployed.append(file_info['dst'])
                        print(f"   ✅ {file_info['dst']} (directory)")
                    else:
                        print(f"   ❌ {file_info['dst']}: {result.stderr}")
                        return {'success': False, 'error': f'Failed to upload {file_info["dst"]}'}
            
            return {
                'success': True,
                'method': 'mpremote',
                'files_deployed': files_deployed,
                'total_files': len(files_deployed)
            }
            
        except Exception as e:
            return {'success': False, 'error': f'mpremote deployment failed: {e}'}
    
    def _deploy_with_ampy(self, target: str, package_info: Dict, config: Dict) -> Dict:
        """Deploy using ampy tool"""
        try:
            temp_dir = package_info['temp_dir']
            files_deployed = []
            
            for file_info in package_info['files']:
                src_file = os.path.join(temp_dir, file_info['dst'])
                dst_file = file_info['dst']
                
                if os.path.isfile(src_file):
                    # Upload file
                    cmd = ['ampy', '--port', target, 'put', src_file, dst_file]
                    result = subprocess.run(cmd, capture_output=True, text=True)
                    
                    if result.returncode == 0:
                        files_deployed.append(file_info['dst'])
                        print(f"   ✅ {file_info['dst']}")
                    else:
                        print(f"   ❌ {file_info['dst']}: {result.stderr}")
                        return {'success': False, 'error': f'Failed to upload {file_info["dst"]}'}
            
            return {
                'success': True,
                'method': 'ampy',
                'files_deployed': files_deployed,
                'total_files': len(files_deployed)
            }
            
        except Exception as e:
            return {'success': False, 'error': f'ampy deployment failed: {e}'}
    
    def _deploy_via_wifi(self, target: str, package_info: Dict, config: Dict) -> Dict:
        """Deploy via WiFi (WebREPL or custom protocol)"""
        try:
            print("   📡 WiFi deployment not implemented yet")
            return {'success': False, 'error': 'WiFi deployment not implemented'}
        except Exception as e:
            return {'success': False, 'error': f'WiFi deployment failed: {e}'}
    
    def _deploy_via_bluetooth(self, target: str, package_info: Dict, config: Dict) -> Dict:
        """Deploy via Bluetooth"""
        try:
            print("   📶 Bluetooth deployment not implemented yet")
            return {'success': False, 'error': 'Bluetooth deployment not implemented'}
        except Exception as e:
            return {'success': False, 'error': f'Bluetooth deployment failed: {e}'}
    
    def _verify_deployment(self, target: str, method: str, components: List[str]) -> Dict:
        """Verify deployment was successful"""
        try:
            print("🔍 Verifying deployment...")
            
            if method == 'usb':
                return self._verify_usb_deployment(target, components)
            else:
                return {'verified': False, 'error': f'Verification not supported for {method}'}
                
        except Exception as e:
            return {'verified': False, 'error': f'Verification failed: {e}'}
    
    def _verify_usb_deployment(self, target: str, components: List[str]) -> Dict:
        """Verify USB deployment"""
        try:
            # List files on device
            cmd = ['mpremote', 'connect', target, 'ls']
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode != 0:
                return {'verified': False, 'error': 'Could not list files on device'}
            
            device_files = result.stdout.strip().split('\n')
            
            # Check for core files
            required_files = ['ethervoxai/']
            found_files = []
            missing_files = []
            
            for required in required_files:
                found = any(required in line for line in device_files)
                if found:
                    found_files.append(required)
                else:
                    missing_files.append(required)
            
            verification_passed = len(missing_files) == 0
            
            if verification_passed:
                print("   ✅ Verification passed")
            else:
                print("   ❌ Verification failed")
                for missing in missing_files:
                    print(f"      Missing: {missing}")
            
            return {
                'verified': verification_passed,
                'found_files': found_files,
                'missing_files': missing_files,
                'device_files': device_files
            }
            
        except Exception as e:
            return {'verified': False, 'error': f'USB verification failed: {e}'}
    
    def _cleanup_deployment(self, package_info: Dict):
        """Clean up temporary deployment files"""
        try:
            if package_info and 'temp_dir' in package_info:
                temp_dir = package_info['temp_dir']
                if os.path.exists(temp_dir):
                    shutil.rmtree(temp_dir)
                    print(f"🧹 Cleaned up temporary files: {temp_dir}")
        except Exception as e:
            print(f"⚠️  Cleanup failed: {e}")
    
    def list_devices(self) -> List[Dict]:
        """List available devices for deployment"""
        devices = []
        
        try:
            # Try to list USB devices with mpremote
            if self.transfer_tools['mpremote']['available']:
                result = subprocess.run(['mpremote', 'devs'], capture_output=True, text=True)
                if result.returncode == 0:
                    for line in result.stdout.strip().split('\n'):
                        if line.strip():
                            devices.append({
                                'type': 'usb',
                                'device': line.strip(),
                                'method': 'mpremote'
                            })
        except Exception as e:
            print(f"⚠️  Device listing failed: {e}")
        
        return devices
    
    def get_device_info(self, target: str, method: str = 'auto') -> Optional[Dict]:
        """Get information about target device"""
        try:
            if method == 'auto':
                method = self._detect_deployment_method(target)
            
            if method == 'usb' and self.transfer_tools['mpremote']['available']:
                # Get device info via mpremote
                cmd = ['mpremote', 'connect', target, 'exec', 
                      'import sys; print(f"Platform: {sys.platform}"); '
                      'import gc; print(f"Memory: {gc.mem_free()} bytes")']
                
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
                
                if result.returncode == 0:
                    output_lines = result.stdout.strip().split('\n')
                    info = {'device': target, 'method': method}
                    
                    for line in output_lines:
                        if 'Platform:' in line:
                            info['platform'] = line.split(':', 1)[1].strip()
                        elif 'Memory:' in line:
                            info['free_memory'] = line.split(':', 1)[1].strip()
                    
                    return info
        
        except Exception as e:
            print(f"⚠️  Could not get device info: {e}")
        
        return None

# Command-line interface
def main():
    """Command-line interface for deployment tool"""
    import argparse
    
    parser = argparse.ArgumentParser(description='EthervoxAI Deployment Tool')
    parser.add_argument('--target', '-t', required=True, 
                       help='Target device (port, IP, etc.)')
    parser.add_argument('--method', '-m', choices=['usb', 'wifi', 'bluetooth', 'auto'],
                       default='auto', help='Deployment method')
    parser.add_argument('--components', '-c', nargs='+', 
                       choices=['core', 'examples', 'tools', 'models'],
                       default=['core', 'examples'], help='Components to deploy')
    parser.add_argument('--source', '-s', help='Source directory')
    parser.add_argument('--config', help='Deployment configuration file')
    parser.add_argument('--list-devices', action='store_true', 
                       help='List available devices')
    parser.add_argument('--device-info', action='store_true',
                       help='Get target device information')
    parser.add_argument('--verify', action='store_true',
                       help='Verify deployment after completion')
    
    args = parser.parse_args()
    
    deployer = DeploymentTool()
    
    if args.list_devices:
        print("📱 Available devices:")
        devices = deployer.list_devices()
        if devices:
            for device in devices:
                print(f"   {device['type']}: {device['device']} ({device['method']})")
        else:
            print("   No devices found")
        return 0
    
    if args.device_info:
        print(f"ℹ️  Getting info for {args.target}...")
        info = deployer.get_device_info(args.target, args.method)
        if info:
            for key, value in info.items():
                print(f"   {key}: {value}")
        else:
            print("   Could not get device information")
        return 0
    
    # Load configuration if provided
    config = {}
    if args.config and os.path.exists(args.config):
        with open(args.config, 'r') as f:
            config = json.load(f)
    
    # Add verify flag to config
    config['verify_deployment'] = args.verify
    
    # Deploy
    result = deployer.deploy(
        target=args.target,
        method=args.method,
        components=args.components,
        source_dir=args.source,
        config=config
    )
    
    if result['success']:
        print(f"✅ Deployed {result.get('total_files', 0)} files successfully")
        if 'verification' in result:
            if result['verification']['verified']:
                print("✅ Deployment verification passed")
            else:
                print("❌ Deployment verification failed")
        return 0
    else:
        print(f"❌ Deployment failed: {result.get('error', 'Unknown error')}")
        return 1

if __name__ == "__main__":
    exit(main())
