import axios from 'axios'

import type {
  ActiveAlarmsResponse,
  DashboardSummary,
  DeviceAlarmsResponse,
  DeviceResponse,
  DevicesResponse,
  TelemetryResponse,
} from '../types/api'

const apiBaseUrl = import.meta.env
  .VITE_API_BASE_URL


if (!apiBaseUrl) {
  throw new Error(
    'VITE_API_BASE_URL is not configured',
  )
}


const apiClient = axios.create({
  baseURL: apiBaseUrl,
  timeout: 10000,
  headers: {
    Accept: 'application/json',
  },
})


export async function getDashboardSummary():
  Promise<DashboardSummary> {
  const response =
    await apiClient.get<DashboardSummary>(
      '/dashboard/summary',
    )

  return response.data
}


export async function getDevices():
  Promise<DevicesResponse> {
  const response =
    await apiClient.get<DevicesResponse>(
      '/devices',
    )

  return response.data
}


export async function getActiveAlarms():
  Promise<ActiveAlarmsResponse> {
  const response =
    await apiClient.get<ActiveAlarmsResponse>(
      '/alarms/active',
    )

  return response.data
}


export async function getDeviceAlarms(
  deviceId: string,
  limit = 100,
): Promise<DeviceAlarmsResponse> {
  const response =
    await apiClient.get<DeviceAlarmsResponse>(
      `/devices/${deviceId}/alarms`,
      {
        params: {
          limit,
        },
      },
    )

  return response.data
}

export async function getDevice(
  deviceId: string,
): Promise<DeviceResponse> {
  const response =
    await apiClient.get<DeviceResponse>(
      `/devices/${deviceId}`,
    )

  return response.data
}


export async function getDeviceTelemetry(
  deviceId: string,
  limit = 100,
  from?: number,
  to?: number,
): Promise<TelemetryResponse> {
  const response =
    await apiClient.get<TelemetryResponse>(
      `/devices/${deviceId}/telemetry`,
      {
        params: {
          limit,
          from,
          to,
        },
      },
    )

  return response.data
}
