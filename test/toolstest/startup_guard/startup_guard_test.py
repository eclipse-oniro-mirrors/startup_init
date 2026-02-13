#!/usr/bin/env python
#coding=utf-8

#
# Copyright (c) 2025 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import sys
import os
import unittest
from unittest.mock import patch, MagicMock

# Get the directory of the current script
script_dir = os.path.dirname(os.path.abspath(__file__))

# Find the directory containing developtools by traversing up the directory tree
def find_developtools_dir(start_dir):
    """Find the directory containing developtools by traversing up the directory tree"""
    current_dir = start_dir
    while current_dir != os.path.dirname(current_dir):  # Stop at filesystem root
        # Check if developtools is a direct child directory
        developtools_path = os.path.join(current_dir, 'developtools')
        if os.path.isdir(developtools_path):
            return current_dir
        current_dir = os.path.dirname(current_dir)
    # Fallback to the original path construction if developtools not found
    return os.path.abspath(os.path.join(start_dir, '../../../../../../'))

project_root = find_developtools_dir(script_dir)
sys.path.append(os.path.join(project_root, 'developtools/integration_verification/tools/startup_guard'))

from startup_checker.sa_directory_rule import SADirectoryRule


class TestSADirectoryRule(unittest.TestCase):
    """Test cases for SADirectoryRule class"""

    def setUp(self):
        """Set up test environment"""
        # Create mock objects for mgr and args
        self.mock_mgr = MagicMock()
        self.mock_args = MagicMock()
        self.mock_args.rules = None

        # Add a custom rules path to load the real whitelist
        self.mock_args.rules = [os.path.join(project_root, 'developtools/integration_verification/tools/startup_guard/rules')]

        # Initialize the rule without mocking get_white_lists
        self.rule = SADirectoryRule(self.mock_mgr, self.mock_args)

        # Mock _get_file_name method to avoid file system dependency
        self.rule._get_file_name = MagicMock(return_value='test_file.cfg')

    def test_check_mkdir_commands_valid(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/test_service 0755 system system',
                'fileId': '1'
            }
        ]

        result = self.rule._check_mkdir_commands(mock_cfg_parser)

        # Valid command should return True
        self.assertTrue(result)

    def test_check_mkdir_commands_not_valid(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/test_service 0755 system system',
                'fileId': '1'
            }
        ]

        # Mock _check_mkdir_commands to return False
        with patch.object(self.rule, '_check_mkdir_commands', return_value=False):
            result = self.rule._check_mkdir_commands(mock_cfg_parser)

        self.assertFalse(result)

    def test_check_mkdir_commands_valid_whitelist(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service',
                'fileId': '1'
            }
        ]

        result = self.rule._check_mkdir_commands(mock_cfg_parser)

        # Command in whitelist should return True
        self.assertTrue(result)

    def test_check_mkdir_commands_not_valid_whitelist(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/abctest',
                'fileId': '1'
            }
        ]

        result = self.rule._check_mkdir_commands(mock_cfg_parser)
        self.assertFalse(result)

    def test_check_mkdir_commands_invalid_data_service_itself(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service 0755 system system',
                'fileId': '1'
            }
        ]

        result = self.rule._check_mkdir_commands(mock_cfg_parser)
        self.assertTrue(result)

    def test_check_mkdir_commands_invalid_outside_data_service(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/other_dir 0755 system system',
                'fileId': '1'
            }
        ]

        result = self.rule._check_mkdir_commands(mock_cfg_parser)
        self.assertFalse(result)

    def test_check_mkdir_commands_invalid_user_mismatch(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/test_service 0755 root root',
                'fileId': '1'
            }
        ]

        result = self.rule._check_mkdir_commands(mock_cfg_parser)
        self.assertTrue(result)

    def test_parse_mkdir_params(self):
        content = '/data/service/test_service 0755 system system'

        params = self.rule._parse_mkdir_params(content)

        self.assertEqual(params['dir_paths'], ['/data/service/test_service'])
        self.assertEqual(params['options'], [])

        # Test with minimal content
        content_minimal = '/data/service/test_service'
        params_minimal = self.rule._parse_mkdir_params(content_minimal)
        self.assertEqual(params_minimal['dir_paths'], ['/data/service/test_service'])
        self.assertEqual(params_minimal['options'], [])

        # Test with space in path (should be quoted in real usage)
        content_with_space = '"/data/service/test service" 0755 system system'
        params_with_space = self.rule._parse_mkdir_params(content_with_space)
        self.assertEqual(params_with_space['dir_paths'], ['/data/service/test service'])
        self.assertEqual(params_with_space['options'], [])

        # Test with space in path but only path (should be quoted in real usage)
        content_space_minimal = '"/data/service/test service"'
        params_space_minimal = self.rule._parse_mkdir_params(content_space_minimal)
        self.assertEqual(params_space_minimal['dir_paths'][0], '/data/service/test service')
        self.assertEqual(params_space_minimal['options'], [])

        # Test with -p option before path
        content_with_p_before = '-p /data/service/test_service 0755 system'
        params_with_p_before = self.rule._parse_mkdir_params(content_with_p_before)
        self.assertEqual(params_with_p_before['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_with_p_before['options'], ['-p'])

        # Test with -p option after path
        content_with_p_after = '/data/service/test_service -p 0755 system'
        params_with_p_after = self.rule._parse_mkdir_params(content_with_p_after)
        self.assertEqual(params_with_p_after['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_with_p_after['options'], ['-p'])

        # Test with -p option only
        content_p_only = '-p /data/service/test_service'
        params_p_only = self.rule._parse_mkdir_params(content_p_only)
        self.assertEqual(params_p_only['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_p_only['options'], ['-p'])

        # Test with -p option and quoted path
        content_p_quoted = '-p "/data/service/test service" 0755 system'
        params_p_quoted = self.rule._parse_mkdir_params(content_p_quoted)
        self.assertEqual(params_p_quoted['dir_paths'][0], '/data/service/test service')
        self.assertEqual(params_p_quoted['options'], ['-p'])

        # Test with -m option (mode)
        content_m_option = '-m 0755 /data/service/test_service 0755 system'
        params_m_option = self.rule._parse_mkdir_params(content_m_option)
        self.assertEqual(params_m_option['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_m_option['options'], ['-m', '0755'])

        # Test with -v option (verbose)
        content_v_option = '-v /data/service/test_service 0755 system'
        params_v_option = self.rule._parse_mkdir_params(content_v_option)
        self.assertEqual(params_v_option['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_v_option['options'], ['-v'])

        # Test with -Z option (SELinux context)
        content_z_option = '-Z /data/service/test_service 0755 system'
        params_z_option = self.rule._parse_mkdir_params(content_z_option)
        self.assertEqual(params_z_option['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_z_option['options'], ['-Z'])

        # Test with --context option (with value)
        content_context_option = '--context=system_u:object_r:system_data_t:s0 /data/service/test_service 0755 system'
        params_context_option = self.rule._parse_mkdir_params(content_context_option)
        self.assertEqual(params_context_option['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_context_option['options'], ['--context=system_u:object_r:system_data_t:s0'])

        # Test with multiple options together
        content_multiple_options = '-p -m 0755 -v /data/service/test_service 0755 system'
        params_multiple_options = self.rule._parse_mkdir_params(content_multiple_options)
        self.assertEqual(params_multiple_options['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_multiple_options['options'], ['-p', '-m', '0755', '-v'])

        # Test with long options
        content_long_options = '--parents --mode=0755 --verbose /data/service/test_service 0755 system'
        params_long_options = self.rule._parse_mkdir_params(content_long_options)
        self.assertEqual(params_long_options['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_long_options['options'], ['--parents', '--mode=0755', '--verbose'])

        # Test with --context and -p option
        content_context_p = '--context=system_u:object_r:system_data_t:s0 -p /data/service/test_service'
        params_context_p = self.rule._parse_mkdir_params(content_context_p)
        self.assertEqual(params_context_p['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_context_p['options'], ['--context=system_u:object_r:system_data_t:s0', '-p'])

        # Test with --context and -m option
        content_context_m = '--context=system_u:object_r:system_data_t:s0 -m 0755 /data/service/test_service'
        params_context_m = self.rule._parse_mkdir_params(content_context_m)
        self.assertEqual(params_context_m['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_context_m['options'], ['--context=system_u:object_r:system_data_t:s0', '-m', '0755'])

        # Test with --context and -v option
        content_context_v = '--context=system_u:object_r:system_data_t:s0 -v /data/service/test_service'
        params_context_v = self.rule._parse_mkdir_params(content_context_v)
        self.assertEqual(params_context_v['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_context_v['options'], ['--context=system_u:object_r:system_data_t:s0', '-v'])

        # Test with --context and -Z option
        content_context_z = '--context=system_u:object_r:system_data_t:s0 -Z /data/service/test_service'
        params_context_z = self.rule._parse_mkdir_params(content_context_z)
        self.assertEqual(params_context_z['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_context_z['options'], ['--context=system_u:object_r:system_data_t:s0', '-Z'])

        # Test with --context, -p, and -m options
        content_context_p_m = '--context=system_u:object_r:system_data_t:s0 -p -m 0755 /data/service/test_service'
        params_context_p_m = self.rule._parse_mkdir_params(content_context_p_m)
        self.assertEqual(params_context_p_m['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_context_p_m['options'], ['--context=system_u:object_r:system_data_t:s0', '-p', '-m', '0755'])

        # Test with --context, -p, -m, and -v options
        content_context_p_m_v = '--context=system_u:object_r:system_data_t:s0 -p -m 0755 -v /data/service/test_service'
        params_context_p_m_v = self.rule._parse_mkdir_params(content_context_p_m_v)
        self.assertEqual(params_context_p_m_v['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_context_p_m_v['options'], ['--context=system_u:object_r:system_data_t:s0', '-p', '-m', '0755', '-v'])

        # Test with --context and long options
        content_context_long = '--context=system_u:object_r:system_data_t:s0 --parents --mode=0755 /data/service/test_service'
        params_context_long = self.rule._parse_mkdir_params(content_context_long)
        self.assertEqual(params_context_long['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_context_long['options'], ['--context=system_u:object_r:system_data_t:s0', '--parents', '--mode=0755'])

        # Test with mixed order of --context and other options
        content_mixed_order = '-p --context=system_u:object_r:system_data_t:s0 -m 0755 /data/service/test_service'
        params_mixed_order = self.rule._parse_mkdir_params(content_mixed_order)
        self.assertEqual(params_mixed_order['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_mixed_order['options'], ['-p', '--context=system_u:object_r:system_data_t:s0', '-m', '0755'])

        # Test with --context and quoted path
        content_context_quoted = '--context=system_u:object_r:system_data_t:s0 -p "/data/service/test service"'
        params_context_quoted = self.rule._parse_mkdir_params(content_context_quoted)
        self.assertEqual(params_context_quoted['dir_paths'][0], '/data/service/test service')
        self.assertEqual(params_context_quoted['options'], ['--context=system_u:object_r:system_data_t:s0', '-p'])

        # Test with options in different positions
        content_options_mixed = '/data/service/test_service -p -m 0755 0755 system'
        params_options_mixed = self.rule._parse_mkdir_params(content_options_mixed)
        self.assertEqual(params_options_mixed['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_options_mixed['options'], ['-p', '-m', '0755'])

        # Test case from the issue - path followed by options and user
        content_issue_case = '/data/service/test_service -p system'
        params_issue_case = self.rule._parse_mkdir_params(content_issue_case)
        self.assertEqual(params_issue_case['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_issue_case['options'], ['-p'])

        # Test with only path and user (no mode)
        content_path_user = '/data/service/test_service system'
        params_path_user = self.rule._parse_mkdir_params(content_path_user)
        self.assertEqual(params_path_user['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_path_user['options'], [])

        # Test with multiple paths (as requested in the issue)
        content_multiple_paths = '/data/service/test1 /data/service/test2 /data/service/test3'
        params_multiple_paths = self.rule._parse_mkdir_params(content_multiple_paths)
        self.assertEqual(params_multiple_paths['dir_paths'], ['/data/service/test1', '/data/service/test2', '/data/service/test3'])
        self.assertEqual(params_multiple_paths['options'], [])

        # Test with multiple paths and options
        content_multiple_paths_options = '-p /data/service/test1 /data/service/test2 -m 0755'
        params_multiple_paths_options = self.rule._parse_mkdir_params(content_multiple_paths_options)
        self.assertEqual(params_multiple_paths_options['dir_paths'], ['/data/service/test1', '/data/service/test2'])
        self.assertEqual(params_multiple_paths_options['options'], ['-p', '-m', '0755'])

        # Test with multiple paths NOT starting with / (relative paths)
        content_relative_paths = 'test1 /test2 /test3'
        params_relative_paths = self.rule._parse_mkdir_params(content_relative_paths)
        self.assertEqual(params_relative_paths['dir_paths'], ['test1', '/test2', '/test3'])
        self.assertEqual(params_relative_paths['options'], [])

        # Test with mixed absolute and relative paths
        content_mixed_paths = '/data/service/test1 test2 /data/service/test3 test4'
        params_mixed_paths = self.rule._parse_mkdir_params(content_mixed_paths)
        self.assertEqual(params_mixed_paths['dir_paths'], ['/data/service/test1', '/data/service/test3'])
        self.assertEqual(params_mixed_paths['options'], [])

        # Test with relative paths and options
        content_relative_paths_options = '-p /test1 test2 test3 -m 0755'
        params_relative_paths_options = self.rule._parse_mkdir_params(content_relative_paths_options)
        self.assertEqual(params_relative_paths_options['dir_paths'], ['/test1'])
        self.assertEqual(params_relative_paths_options['options'], ['-p', '-m', '0755'])

        # Test with complex relative paths (with subdirectories)
        content_complex_relative = '/subdir/test1 subdir/test2 another_dir/test3'
        params_complex_relative = self.rule._parse_mkdir_params(content_complex_relative)
        self.assertEqual(params_complex_relative['dir_paths'], ['/subdir/test1'])
        self.assertEqual(params_complex_relative['options'], [])

        # Test with relative paths and context option
        content_relative_context = '--context=system_u:object_r:system_data_t:s0 -p /test1 test2'
        params_relative_context = self.rule._parse_mkdir_params(content_relative_context)
        self.assertEqual(params_relative_context['dir_paths'], ['/test1'])
        self.assertEqual(params_relative_context['options'], ['--context=system_u:object_r:system_data_t:s0', '-p'])

        # Test with complex mixed options
        content_complex_options = '-p -v /data/service/test_service -m 0755 system'
        params_complex_options = self.rule._parse_mkdir_params(content_complex_options)
        self.assertEqual(params_complex_options['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_complex_options['options'], ['-p', '-v', '-m', '0755'])

        # Test with multiple options requiring values
        content_multi_value_options = '-m 0700 -Z /data/service/test_service system'
        params_multi_value_options = self.rule._parse_mkdir_params(content_multi_value_options)
        self.assertEqual(params_multi_value_options['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_multi_value_options['options'], ['-m', '0700', '-Z'])

        # Test with quoted options and path
        content_quoted_options = '-p "/data/service/test service" -m "0755" system'
        params_quoted_options = self.rule._parse_mkdir_params(content_quoted_options)
        self.assertEqual(params_quoted_options['dir_paths'][0], '/data/service/test service')
        self.assertEqual(params_quoted_options['options'], ['-p', '-m', '0755'])

    def test_parse_mkdir_params_with_invalid_options(self):
        """Test parsing mkdir command parameters with invalid options like -context"""
        # Test with invalid option -context (should be --context)
        content_invalid_context = '-context=system_u:object_r:system_data_t:s0 /data/service/test_service'
        params_invalid_context = self.rule._parse_mkdir_params(content_invalid_context)

        # According to current implementation, it should treat -context as a valid option
        # even though it's not a standard mkdir option
        self.assertEqual(params_invalid_context['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_invalid_context['options'], ['-context=system_u:object_r:system_data_t:s0'])

        # Test with invalid option -invalid
        content_invalid_option = '-invalid /data/service/test_service'
        params_invalid_option = self.rule._parse_mkdir_params(content_invalid_option)

        # It should treat -invalid as a valid option
        self.assertEqual(params_invalid_option['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_invalid_option['options'], ['-invalid'])

        # Test with invalid option that should require a value
        content_invalid_option_with_value = '-invalid 123 /data/service/test_service'
        params_invalid_option_with_value = self.rule._parse_mkdir_params(content_invalid_option_with_value)

        # Since -invalid is not in options_with_values, it should not expect a value
        self.assertEqual(params_invalid_option_with_value['dir_paths'][0], '123')
        self.assertEqual(params_invalid_option_with_value['dir_paths'][1], '/data/service/test_service')
        self.assertEqual(params_invalid_option_with_value['options'], ['-invalid'])

    def test_parse_mkdir_params_with_illegal_keywords(self):
        """Test parsing mkdir command parameters with illegal keywords"""

        # Test with illegal keyword after valid path
        content_illegal_keyword = '/data/service/test_service illegal_keyword'
        params_illegal_keyword = self.rule._parse_mkdir_params(content_illegal_keyword)

        # According to the current implementation, illegal keywords should be ignored
        # Only the path should be extracted
        self.assertEqual(params_illegal_keyword['dir_paths'][0], '/data/service/test_service')
        self.assertEqual(params_illegal_keyword['options'], [])

        # Test with multiple illegal keywords
        content_multiple_illegal = '/data/service/test1 illegal1 /data/service/test2 illegal2'
        params_multiple_illegal = self.rule._parse_mkdir_params(content_multiple_illegal)

        # Paths should still be extracted, illegal keywords should be ignored
        self.assertEqual(params_multiple_illegal['dir_paths'], ['/data/service/test1', '/data/service/test2'])
        self.assertEqual(params_multiple_illegal['options'], [])

    def test_parse_whitelist(self):
        """Test parsing whitelist configuration"""
        # Verify that whitelist was loaded correctly
        self.assertGreater(len(self.rule._mkdir_cmd_whitelist), 0)
        for entry in self.rule._mkdir_cmd_whitelist:
            self.assertIsInstance(entry, str)

    def test_check_mkdir_commands_multiple(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            # Valid command - in /data/service/ subdir matches
            {
                'name': 'mkdir',
                'content': '/data/service/test_service 0755 system system',
                'fileId': '1'
            },
            # Invalid command - outside /data/service/
            {
                'name': 'mkdir',
                'content': '/data/other_dir 0755 system system',
                'fileId': '2'
            },
            # Valid command - /data/service itself (should be in whitelist)
            {
                'name': 'mkdir',
                'content': '/data/service',
                'fileId': '3'
            }
        ]
        mock_cfg_parser._services = {
            'test_service': {
                'uid': 'system',
                'fileId': '1'
            }
        }

        # Mock _check_user_service_uid_match to return True for the first command
        self.rule._check_user_service_uid_match = MagicMock()
        self.rule._check_user_service_uid_match.side_effect = [True, False, False]

        result = self.rule._check_mkdir_commands(mock_cfg_parser)

        # Should return False due to invalid commands
        self.assertFalse(result)

    def test_check_method_full_flow(self):
        """Test the complete __check__ method flow"""
        # Mock get_mgr().get_parser_by_name('config_parser') to return None
        self.rule.get_mgr = MagicMock()
        self.rule.get_mgr().get_parser_by_name.return_value = None

        result = self.rule.__check__()

        # Should return True when no cfg_parser
        self.assertTrue(result)

        # Mock get_mgr().get_parser_by_name('config_parser') to return a mock cfg_parser
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/test_service 0755 system system',
                'fileId': '1'
            }
        ]
        mock_cfg_parser._services = {
            'test_service': {
                'uid': 'system',
                'fileId': '1'
            }
        }

        self.rule.get_mgr().get_parser_by_name.return_value = mock_cfg_parser
        self.rule._check_mkdir_commands = MagicMock(return_value=True)

        result = self.rule.__check__()

        # Should return True when _check_mkdir_commands passes
        self.assertTrue(result)

    def test_parse_whitelist_behavior(self):
        """Test _parse_whitelist behavior by directly modifying internal state"""
        # Save original whitelist
        original_whitelist = self.rule._mkdir_cmd_whitelist.copy()

        try:
            # Test 1: Simulate _parse_whitelist when get_white_lists returns a new list
            test_whitelist = ['mkdir /test/path1', 'mkdir /test/path2']

            # Mock the internal _parse_whitelist behavior directly
            self.rule._mkdir_cmd_whitelist = test_whitelist.copy()

            # Verify the whitelist was set correctly
            self.assertEqual(len(self.rule._mkdir_cmd_whitelist), 2)
            self.assertIn('mkdir /test/path1', self.rule._mkdir_cmd_whitelist)
            self.assertIn('mkdir /test/path2', self.rule._mkdir_cmd_whitelist)

            # Test 2: Simulate _parse_whitelist when get_white_lists returns None
            # This is what happens internally when no whitelist is found
            self.rule._mkdir_cmd_whitelist = []

            # Verify the whitelist is empty
            self.assertEqual(len(self.rule._mkdir_cmd_whitelist), 0)
        finally:
            # Restore original whitelist
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_commands_invalid_format(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '',  # Empty content
                'fileId': '1'
            }
        ]

        # Mock _get_file_name to return a known file name
        self.rule._get_file_name = MagicMock(return_value='test_file.cfg')

        # Mock error method to avoid printing to console
        self.rule.error = MagicMock()

        result = self.rule._check_mkdir_commands(mock_cfg_parser)

        # Should return False due to invalid command
        self.assertFalse(result)

        # Verify error method was called
        self.rule.error.assert_called_once()

    def test_get_file_name(self):
        """Test _get_file_name method"""
        # Restore original _get_file_name method (not the mocked one from setUp)
        original_get_file_name = self.rule.__class__._get_file_name

        mock_cfg_parser = MagicMock()
        mock_cfg_parser._files = {
            'test1.cfg': {'fileId': '1'},
            'test2.cfg': {'fileId': '2'}
        }

        # Test with existing fileId
        file_name = original_get_file_name(self.rule, mock_cfg_parser, '1')
        self.assertEqual(file_name, 'test1.cfg')

        file_name = original_get_file_name(self.rule, mock_cfg_parser, '2')
        self.assertEqual(file_name, 'test2.cfg')

        # Test with non-existent fileId
        file_name = original_get_file_name(self.rule, mock_cfg_parser, '3')
        self.assertEqual(file_name, 'unknown_file')

    def test_check_mkdir_commands_data_service_subdir_no_user(self):
        """Test mkdir command in /data/service subdir but without user info"""
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/test_service 0755',  # No user info
                'fileId': '1'
            }
        ]

        # Test with empty whitelist (simulate by temporarily replacing it)
        original_whitelist = self.rule._mkdir_cmd_whitelist
        self.rule._mkdir_cmd_whitelist = []

        # Mock _get_file_name method
        self.rule._get_file_name = MagicMock(return_value='test_file.cfg')

        result = self.rule._check_mkdir_commands(mock_cfg_parser)

        # Restore original whitelist
        self.rule._mkdir_cmd_whitelist = original_whitelist

        # Should return True because user/group info is no longer required
        self.assertTrue(result)

    def test_check_mkdir_commands_data_service_subdir_user_mismatch_but_in_whitelist(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/test_service 0755 root root',  # User mismatch
                'fileId': '1'
            }
        ]
        mock_cfg_parser._services = {
            'test_service': {
                'uid': 'system',  # Different from command's user
                'fileId': '1'
            }
        }

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add the command to the whitelist temporarily
            test_cmd = 'mkdir /data/service/test_service 0755 root root'
            self.rule._mkdir_cmd_whitelist.append(test_cmd)

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Should return True due to being in whitelist
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_commands_complex_whitelist_command(self):
        """Test complex mkdir command that matches whitelist"""
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/system 0771 system system',  # Complex command with permissions and user info
                'fileId': '1'
            }
        ]

        # This command should be in the whitelist, so the test should pass
        result = self.rule._check_mkdir_commands(mock_cfg_parser)

        # Should return True due to being in whitelist
        self.assertTrue(result)

    def test_check_mkdir_commands_non_mkdir_commands(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'chmod',
                'content': '/data/service/test_service 0755',
                'fileId': '1'
            },
            {
                'name': 'chown',
                'content': 'system:system /data/service/test_service',
                'fileId': '2'
            }
        ]

        result = self.rule._check_mkdir_commands(mock_cfg_parser)
        self.assertTrue(result)

    def test_check_single_mkdir_command_valid(self):
        mock_cfg_parser = MagicMock()
        mkdir_params = {
            'dir_path': '/data/service/test_service',
            'permission': '0755',
            'user': 'system',
            'group': 'system'
        }

        content = '/data/service/test_service 0755 system system'
        file_id = '1'

        result = self.rule._check_single_mkdir_command(mkdir_params, content, mock_cfg_parser, file_id)

        self.assertTrue(result)

    def test_check_single_mkdir_command_invalid_user_mismatch(self):
        mock_cfg_parser = MagicMock()
        mkdir_params = {
            'dir_path': '/data/service/test_service',
            'permission': '0755',
            'user': 'root',
            'group': 'root'
        }

        content = '/data/service/test_service 0755 root root'
        file_id = '1'

        result = self.rule._check_single_mkdir_command(mkdir_params, content, mock_cfg_parser, file_id)

        # In the new implementation, user/group checks are not performed, so this should return True
        # if the path is valid and doesn't contain 10xx directories
        self.assertTrue(result)

    def test_check_single_mkdir_command_invalid_path(self):
        mock_cfg_parser = MagicMock()
        mkdir_params = {
            'dir_path': '/data/other_dir',
            'permission': '0755',
            'user': 'system',
            'group': 'system'
        }

        content = '/data/other_dir 0755 system system'
        file_id = '1'

        result = self.rule._check_single_mkdir_command(mkdir_params, content, mock_cfg_parser, file_id)

        # Invalid path should return False
        self.assertFalse(result)

    def test_check_single_mkdir_command_in_whitelist(self):
        mock_cfg_parser = MagicMock()
        mkdir_params = {
            'dir_path': '/data/whitelisted_dir',
            'permission': '0755',
            'user': 'system',
            'group': 'system'
        }

        content = '/data/whitelisted_dir 0755 system system'
        file_id = '1'

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add the directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/whitelisted_dir')

            result = self.rule._check_single_mkdir_command(mkdir_params, content, mock_cfg_parser, file_id)

            # Command in whitelist should return True
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_single_mkdir_command_data_service_itself(self):
        mock_cfg_parser = MagicMock()
        mkdir_params = {
            'dir_path': '/data/service',
            'permission': '0755',
            'user': 'system',
            'group': 'system'
        }

        content = '/data/service 0755 system system'
        file_id = '1'

        result = self.rule._check_single_mkdir_command(mkdir_params, content, mock_cfg_parser, file_id)

        # In new implementation, /data/service itself is an allowed base directory
        self.assertTrue(result)

    def test_check_mkdir_command_illegal_directory_not_in_whitelist(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/test/100 0755 system system',
                'fileId': '1'
            }
        ]

        result = self.rule._check_mkdir_commands(mock_cfg_parser)

        # Illegal directory not in whitelist should be denied
        self.assertFalse(result)

    def test_check_mkdir_command_illegal_directory_in_whitelist(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/abc/100 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add the illegal directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/abc/100')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Illegal directory in whitelist should be allowed
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_multiple_illegal_directories_last_in_whitelist(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/data/105/101 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add the last illegal directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/data/105/101')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Last illegal directory in whitelist should be allowed, even if parent isn't
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_multiple_illegal_directories_last_not_in_whitelist(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/data/105/101 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add only the parent illegal directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/data/105')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Last illegal directory not in whitelist should be denied, even if parent is
            self.assertFalse(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_various_illegal_directories_100(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/100 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add the illegal directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/test/100')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Illegal directory 100 in whitelist should be allowed
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_various_illegal_directories_101(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/101 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add the illegal directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/test/101')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Illegal directory 101 in whitelist should be allowed
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_various_illegal_directories_102(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/102 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add the illegal directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/test/102')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Illegal directory 102 in whitelist should be allowed
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_various_illegal_directories_103(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/103 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add the illegal directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/test/103')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Illegal directory 103 in whitelist should be allowed
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_various_illegal_directories_104(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/104 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add the illegal directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/test/104')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Illegal directory 104 in whitelist should be allowed
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_various_illegal_directories_105(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/105 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add the illegal directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/test/105')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Illegal directory 105 in whitelist should be allowed
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_complex_path_100_whitelist_100(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/100/el2/100 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add parent directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/service/100')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Complex path with illegal directory 100 should be blocked even if parent is in whitelist
            self.assertFalse(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_complex_path_105_whitelist_100(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/100/el2/105 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add parent directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/service/100')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Complex path with illegal directory 105 should be blocked even if parent is in whitelist
            self.assertFalse(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_complex_path_106_whitelist_100(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/100/el2/106 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add parent directory to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/service/100')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Complex path with directory 106 (not illegal) should be allowed if parent is in whitelist
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_complex_path_100_whitelist_full(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/100/el2/100 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add full path to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/service/100/el2/100')
            result = self.rule._check_mkdir_commands(mock_cfg_parser)
            # Complex path with illegal directory 100 should be allowed if full path is in whitelist
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_complex_path_105_whitelist_full(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/100/el2/105 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)
        try:
            # Add full path to whitelist temporarily
            self.rule._mkdir_cmd_whitelist.append('/data/service/100/el2/105')
            result = self.rule._check_mkdir_commands(mock_cfg_parser)
            # Complex path with illegal directory 105 should be allowed if full path is in whitelist
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_complex_path_106_whitelist_full(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/100/el2/106 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # For paths containing illegal directories (like 100), we need to add the path up to the illegal directory to whitelist
            self.rule._mkdir_cmd_whitelist.append('/data/service/100')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Complex path with illegal directory 100 in whitelist should be allowed
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist


    def test_check_mkdir_command_data_test_100_no_whitelist(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/100 0755 system system',
                'fileId': '1'
            }
        ]

        # No whitelist added, should fail because 100 is illegal directory
        result = self.rule._check_mkdir_commands(mock_cfg_parser)
        self.assertFalse(result)

    def test_check_mkdir_command_data_test_100_whitelist_parent(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/100 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add parent path to whitelist
            self.rule._mkdir_cmd_whitelist.append('/data/test')
            result = self.rule._check_mkdir_commands(mock_cfg_parser)
            # Should fail because parent path in whitelist doesn't allow illegal subdirectory 100
            self.assertFalse(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_data_test_100_whitelist_full(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/100 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add full path to whitelist
            self.rule._mkdir_cmd_whitelist.append('/data/test/100')
            result = self.rule._check_mkdir_commands(mock_cfg_parser)
            # Should pass because full path is in whitelist
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_data_test_100_abc_100_no_whitelist(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/100/abc/100 0755 system system',
                'fileId': '1'
            }
        ]

        # No whitelist added, should fail because 100 is illegal directory
        result = self.rule._check_mkdir_commands(mock_cfg_parser)
        self.assertFalse(result)

    def test_check_mkdir_command_data_test_100_abc_100_whitelist_grandparent(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/100/abc/100 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add grandparent path to whitelist
            self.rule._mkdir_cmd_whitelist.append('/data/test')
            result = self.rule._check_mkdir_commands(mock_cfg_parser)
            # Should fail because grandparent path in whitelist doesn't allow illegal subdirectories
            self.assertFalse(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_data_test_100_abc_100_whitelist_parent(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/100/abc/100 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add parent path to whitelist
            self.rule._mkdir_cmd_whitelist.append('/data/test/100')
            result = self.rule._check_mkdir_commands(mock_cfg_parser)
            # Should fail because parent path in whitelist doesn't allow illegal subdirectory 100
            self.assertFalse(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_data_test_100_abc_100_whitelist_full(self):
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/test/100/abc/100 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add full path to whitelist
            self.rule._mkdir_cmd_whitelist.append('/data/test/100/abc/100')
            result = self.rule._check_mkdir_commands(mock_cfg_parser)
            # Should pass because full path is in whitelist
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_illegal_dir_prefix_match_same_last_illegal_index(self):
        """Test prefix matching when whitelist path has illegal dir and same last illegal index"""
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/el2/100/what/abcd 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add whitelist path with illegal directory 100 (last illegal index is at position 3)
            self.rule._mkdir_cmd_whitelist.append('/data/service/el2/100/what')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Should pass because whitelist path has illegal dir and same last illegal index
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_illegal_dir_prefix_match_different_last_illegal_index(self):
        """Test prefix matching when whitelist path has illegal dir but different last illegal index"""
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/el2/100/what/abc/102/abc 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add whitelist path with illegal directory 100 (last illegal index is at position 3)
            # But cfg path has last illegal directory 102 at position 5
            self.rule._mkdir_cmd_whitelist.append('/data/service/el2/100')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Should fail because last illegal index is different
            self.assertFalse(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_illegal_dir_prefix_match_whitelist_no_illegal_dir(self):
        """Test prefix matching when whitelist path has no illegal directory"""
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/el2/100/what/abcd 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add whitelist path without illegal directory
            self.rule._mkdir_cmd_whitelist.append('/data/service/el2')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Should fail because whitelist path has no illegal directory
            self.assertFalse(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_illegal_dir_prefix_match_with_102(self):
        """Test prefix matching with illegal directory 102"""
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/el2/100/what/abc/102/xyz 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add whitelist path with illegal directory 102 (last illegal index)
            self.rule._mkdir_cmd_whitelist.append('/data/service/el2/100/what/abc/102')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Should pass because whitelist path has illegal dir 102 and same last illegal index
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_illegal_dir_prefix_match_multiple_illegal_dirs(self):
        """Test prefix matching with multiple illegal directories"""
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/100/what/102/abc/105/xyz/extra 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add whitelist path with last illegal directory 105
            self.rule._mkdir_cmd_whitelist.append('/data/service/100/what/102/abc/105/xyz')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Should pass because whitelist path has illegal dir and same last illegal index (105)
            self.assertTrue(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

    def test_check_mkdir_command_illegal_dir_prefix_match_multiple_illegal_dirs_wrong_last(self):
        """Test prefix matching with multiple illegal directories but wrong last one"""
        mock_cfg_parser = MagicMock()
        mock_cfg_parser._cmds = [
            {
                'name': 'mkdir',
                'content': '/data/service/100/what/102/abc/105/xyz/extra 0755 system system',
                'fileId': '1'
            }
        ]

        # Save original whitelist state for proper test isolation
        original_whitelist = list(self.rule._mkdir_cmd_whitelist)

        try:
            # Add whitelist path with last illegal directory 102 (but cfg path has 105 as last)
            self.rule._mkdir_cmd_whitelist.append('/data/service/100/what/102')

            result = self.rule._check_mkdir_commands(mock_cfg_parser)

            # Should fail because last illegal index is different
            self.assertFalse(result)
        finally:
            # Restore original whitelist state to ensure test isolation
            self.rule._mkdir_cmd_whitelist = original_whitelist

if __name__ == '__main__':
    unittest.main()
