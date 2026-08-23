import { useQuery } from '@tanstack/react-query'
import {
  AlertTriangle,
  CheckCircle2,
  Cloud,
  Database,
  HardDrive,
  RefreshCw,
  Wifi,
  XCircle,
} from 'lucide-react'

import {
  getDevices,
} from '../api/dashboardApi'
import type {
  Device,
} from '../types/api'


function deviceIsHealthy(
  device: Device,
) {
  return (
    device.status?.toLowerCase()
      === 'online'
    && device.wifi_connected === true
    && device.mqtt_ready === true
    && device.storage_mounted === true
    && device.storage_degraded !== true
  )
}


function getDeviceWarnings(
  device: Device,
) {
  const warnings: string[] = []

  if (
    device.status?.toLowerCase()
    !== 'online'
  ) {
    warnings.push('Device offline')
  }

  if (!device.wifi_connected) {
    warnings.push('Wi-Fi disconnected')
  }

  if (!device.mqtt_connected) {
    warnings.push('MQTT disconnected')
  }

  if (!device.mqtt_ready) {
    warnings.push('MQTT not ready')
  }

  if (!device.storage_mounted) {
    warnings.push('Storage unavailable')
  }

  if (device.storage_degraded) {
    warnings.push('Storage degraded')
  }

  if (
    Number(device.pending_bytes ?? 0)
    > 0
  ) {
    warnings.push(
      `${device.pending_bytes} pending bytes`,
    )
  }

  if (
    Number(device.cloud_errors ?? 0)
    > 0
  ) {
    warnings.push(
      `${device.cloud_errors} cloud errors`,
    )
  }

  return warnings
}


function formatBytes(
  value?: number,
) {
  const bytes = value ?? 0

  if (bytes < 1024) {
    return `${bytes} B`
  }

  if (bytes < 1024 * 1024) {
    return `${(
      bytes / 1024
    ).toFixed(1)} KB`
  }

  return `${(
    bytes / (1024 * 1024)
  ).toFixed(1)} MB`
}


function DeviceHealthPage() {
  const devicesQuery = useQuery({
    queryKey: ['devices'],
    queryFn: getDevices,
    refetchInterval: 15000,
  })

  function refreshHealth() {
    void devicesQuery.refetch()
  }

  if (devicesQuery.isLoading) {
    return (
      <div className="flex min-h-80 items-center justify-center">
        <RefreshCw
          className="animate-spin text-cyan-400"
          size={32}
        />
      </div>
    )
  }

  if (
    devicesQuery.isError
    || !devicesQuery.data
  ) {
    return (
      <div className="rounded-2xl border border-red-500/20 bg-red-500/5 p-8">
        <h2 className="text-xl font-semibold text-red-400">
          Unable to load device health
        </h2>

        <button
          type="button"
          onClick={refreshHealth}
          className="mt-5 rounded-xl bg-red-500 px-5 py-2.5 text-white"
        >
          Try again
        </button>
      </div>
    )
  }

  const devices =
    devicesQuery.data.devices

  const healthyDevices =
    devices.filter(
      deviceIsHealthy,
    ).length

  const attentionDevices =
    devices.length
    - healthyDevices

  const degradedStorage =
    devices.filter(
      (device) =>
        device.storage_degraded
        || !device.storage_mounted,
    ).length

  return (
    <section>
      <div className="flex flex-col justify-between gap-4 md:flex-row md:items-center">
        <div>
          <h1 className="text-3xl font-bold text-white">
            Device Health
          </h1>

          <p className="mt-2 text-slate-400">
            Connectivity, storage and cloud delivery diagnostics.
          </p>
        </div>

        <button
          type="button"
          onClick={refreshHealth}
          disabled={
            devicesQuery.isFetching
          }
          className="flex items-center justify-center gap-2 rounded-xl border border-slate-700 bg-slate-900 px-4 py-2.5 text-sm text-slate-300 disabled:opacity-50"
        >
          <RefreshCw
            size={17}
            className={
              devicesQuery.isFetching
                ? 'animate-spin'
                : ''
            }
          />

          Refresh
        </button>
      </div>

      <div className="mt-8 grid gap-4 sm:grid-cols-3">
        <div className="rounded-2xl border border-emerald-500/20 bg-emerald-500/5 p-5">
          <CheckCircle2 className="text-emerald-400" />

          <p className="mt-4 text-sm text-slate-400">
            Healthy Devices
          </p>

          <p className="mt-2 text-3xl font-bold text-emerald-400">
            {healthyDevices}
          </p>
        </div>

        <div className="rounded-2xl border border-amber-500/20 bg-amber-500/5 p-5">
          <AlertTriangle className="text-amber-400" />

          <p className="mt-4 text-sm text-slate-400">
            Needs Attention
          </p>

          <p className="mt-2 text-3xl font-bold text-amber-400">
            {attentionDevices}
          </p>
        </div>

        <div className="rounded-2xl border border-red-500/20 bg-red-500/5 p-5">
          <HardDrive className="text-red-400" />

          <p className="mt-4 text-sm text-slate-400">
            Storage Problems
          </p>

          <p className="mt-2 text-3xl font-bold text-red-400">
            {degradedStorage}
          </p>
        </div>
      </div>

      <div className="mt-8 grid gap-6">
        {devices.map((device) => {
          const healthy =
            deviceIsHealthy(device)

          const warnings =
            getDeviceWarnings(device)

          return (
            <article
              key={device.device_id}
              className={[
                'overflow-hidden rounded-2xl border bg-slate-900/60',
                healthy
                  ? 'border-emerald-500/20'
                  : 'border-amber-500/30',
              ].join(' ')}
            >
              <div className="flex flex-col justify-between gap-4 border-b border-slate-800 px-6 py-5 md:flex-row md:items-center">
                <div className="flex items-center gap-4">
                  <div
                    className={[
                      'flex size-12 items-center justify-center rounded-xl',
                      healthy
                        ? 'bg-emerald-500/15 text-emerald-400'
                        : 'bg-amber-500/15 text-amber-400',
                    ].join(' ')}
                  >
                    {healthy
                      ? (
                        <CheckCircle2
                          size={24}
                        />
                      )
                      : (
                        <AlertTriangle
                          size={24}
                        />
                      )}
                  </div>

                  <div>
                    <h2 className="font-semibold text-white">
                      {device.device_id}
                    </h2>

                    <p className="mt-1 text-sm text-slate-500">
                      Firmware {device.firmware_version ?? 'Unknown'}
                    </p>
                  </div>
                </div>

                <span
                  className={[
                    'w-fit rounded-full px-3 py-1 text-xs font-medium',
                    healthy
                      ? 'bg-emerald-500/15 text-emerald-400'
                      : 'bg-amber-500/15 text-amber-400',
                  ].join(' ')}
                >
                  {healthy
                    ? 'Healthy'
                    : 'Needs attention'}
                </span>
              </div>

              <div className="grid gap-px bg-slate-800 sm:grid-cols-2 xl:grid-cols-4">
                <div className="bg-slate-900 p-5">
                  <Wifi
                    className={
                      device.wifi_connected
                        ? 'text-emerald-400'
                        : 'text-red-400'
                    }
                    size={20}
                  />

                  <p className="mt-3 text-xs text-slate-500">
                    Wi-Fi
                  </p>

                  <p className="mt-1 font-medium text-white">
                    {device.wifi_connected
                      ? 'Connected'
                      : 'Disconnected'}
                  </p>
                </div>

                <div className="bg-slate-900 p-5">
                  <Cloud
                    className={
                      device.mqtt_ready
                        ? 'text-emerald-400'
                        : 'text-red-400'
                    }
                    size={20}
                  />

                  <p className="mt-3 text-xs text-slate-500">
                    MQTT
                  </p>

                  <p className="mt-1 font-medium text-white">
                    {device.mqtt_ready
                      ? 'Ready'
                      : 'Not ready'}
                  </p>
                </div>

                <div className="bg-slate-900 p-5">
                  <Database
                    className={
                      device.storage_mounted
                        && !device.storage_degraded
                        ? 'text-emerald-400'
                        : 'text-red-400'
                    }
                    size={20}
                  />

                  <p className="mt-3 text-xs text-slate-500">
                    Journal Size
                  </p>

                  <p className="mt-1 font-medium text-white">
                    {formatBytes(
                      device.journal_size_bytes,
                    )}
                  </p>
                </div>

                <div className="bg-slate-900 p-5">
                  <HardDrive
                    className={
                      Number(
                        device.pending_bytes
                        ?? 0,
                      ) === 0
                        ? 'text-emerald-400'
                        : 'text-amber-400'
                    }
                    size={20}
                  />

                  <p className="mt-3 text-xs text-slate-500">
                    Pending Data
                  </p>

                  <p className="mt-1 font-medium text-white">
                    {formatBytes(
                      device.pending_bytes,
                    )}
                  </p>
                </div>
              </div>

              <div className="grid gap-4 p-6 sm:grid-cols-2 lg:grid-cols-5">
                <div>
                  <p className="text-xs text-slate-500">
                    Records Written
                  </p>

                  <p className="mt-1 font-medium text-white">
                    {device.journal_records_written ?? 0}
                  </p>
                </div>

                <div>
                  <p className="text-xs text-slate-500">
                    Cursor Offset
                  </p>

                  <p className="mt-1 font-medium text-white">
                    {device.cursor_offset ?? 0}
                  </p>
                </div>

                <div>
                  <p className="text-xs text-slate-500">
                    Last ACK Record
                  </p>

                  <p className="mt-1 font-medium text-white">
                    {device.last_acknowledged_record_id ?? 0}
                  </p>
                </div>

                <div>
                  <p className="text-xs text-slate-500">
                    Disconnections
                  </p>

                  <p className="mt-1 font-medium text-white">
                    {device.cloud_disconnections ?? 0}
                  </p>
                </div>

                <div>
                  <p className="text-xs text-slate-500">
                    Cloud Errors
                  </p>

                  <p className="mt-1 font-medium text-white">
                    {device.cloud_errors ?? 0}
                  </p>
                </div>
              </div>

              {warnings.length > 0 && (
                <div className="border-t border-slate-800 px-6 py-5">
                  <div className="flex flex-wrap gap-2">
                    {warnings.map(
                      (warning) => (
                        <span
                          key={warning}
                          className="inline-flex items-center gap-2 rounded-lg bg-amber-500/10 px-3 py-2 text-xs text-amber-400"
                        >
                          <XCircle
                            size={14}
                          />

                          {warning}
                        </span>
                      ),
                    )}
                  </div>
                </div>
              )}
            </article>
          )
        })}

        {devices.length === 0 && (
          <div className="rounded-2xl border border-dashed border-slate-700 p-12 text-center text-slate-500">
            No devices found.
          </div>
        )}
      </div>
    </section>
  )
}


export default DeviceHealthPage