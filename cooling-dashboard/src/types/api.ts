export interface DashboardSummary {
  total_devices: number
  online_devices: number
  offline_devices: number
  active_alarms: number
  critical_alarms: number
}

export interface Device {
  device_id: string
  firmware_version?: string
  status?: 'online' | 'offline' | string
  uptime_ms?: number

  wifi_connected?: boolean
  mqtt_connected?: boolean
  mqtt_ready?: boolean

  storage_mounted?: boolean
  storage_degraded?: boolean

  journal_records_written?: number
  journal_size_bytes?: number
  cursor_offset?: number
  last_acknowledged_record_id?: number
  pending_bytes?: number

  cloud_disconnections?: number
  cloud_errors?: number

  [key: string]: unknown
}

export interface Alarm {
  device_id: string
  record_id: number

  alarm_instance_id?: string
  alarm_code?: string
  severity?: string
  transition?: string

  transition_sequence?: number
  source_signal?: string
  source_quality?: string
  source_value?: number

  received_at?: string
  received_at_ms?: number
  observed_at_ms?: number | null

  [key: string]: unknown
}

export interface DevicesResponse {
  count: number
  devices: Device[]
}

export interface ActiveAlarmsResponse {
  count: number
  critical_count: number
  alarms: Alarm[]
}

export interface DeviceAlarmsResponse {
  device_id: string
  count: number
  alarms: Alarm[]
}

export interface DeviceResponse {
  device: Device
}

export interface TelemetrySignal {
  signal_id?: number
  name?: string
  value?: number | boolean | string | null
  unit?: string
  quality?: string
  has_value?: boolean
  sample_uptime_ms?: number

  [key: string]: unknown
}

export interface TelemetryRecord {
  device_id: string
  firmware_version?: string
  message_type?: string
  schema_version?: number

  snapshot_sequence?: number
  received_at_ms?: number
  created_uptime_ms?: number
  observed_at_ms?: number | null

  signals?: TelemetrySignal[]

  [key: string]: unknown
}

export interface TelemetryResponse {
  device_id: string
  count: number
  telemetry: TelemetryRecord[]
}