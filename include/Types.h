#pragma once

enum class DeviceStatus {
  OK,
  OFFLINE,
  ERROR,
  INVALID
};

enum class PublishResult {
  SUCCESS,
  RETRY,
  FAILED
};
