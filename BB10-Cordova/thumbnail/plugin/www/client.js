/*
 * Copyright (c) 2014 BlackBerry Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
var _self = {},
    _ID = "cordova-plugin-thumbnail",
	exec = cordova.require("cordova/exec");

_self.createThumbNail = function (path, options) {
	var result,
    success = function (data, response) {
        result = data;
    },
    fail = function (data, response) {
        console.log("Error: " + data);
    };
	options.path = path;
	exec(success, fail, _ID, "createThumbNail", options);
    var path = "file://" + result;
	var image = new Image();
	image.src = path;
	return image;
};

module.exports = _self;