import { useQuery } from '@tanstack/react-query'
import {
  Boxes,
  Cloud,
  Database,
  RefreshCw,
  Wifi,
  WifiOff,
} from 'lucide-react'

import { Link } from 'react-router-dom'

import { getDevices } from '../api/dashboardApi'


function booleanStatus(
  value?: boolean,
) {
  return value === true
    ? 'text-emerald-400'
    : 'text-red-400'
}


function DevicesPage() {
  const devicesQuery = useQuery({
    queryKey: ['devices'],
    queryFn: getDevices,
    refetchInterval: 15000,
  })

  function refreshDevices() {
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
          Unable to load devices
        </h2>

        <button
          type="button"
          onClick={refreshDevices}
          className="mt-5 rounded-xl bg-red-500 px-5 py-2.5 text-white"
        >
          Try again
        </button>
      </div>
    )
  }

  const devices =
    devicesQuery.data.devices

  const onlineCount = devices.filter(
    (device) =>
      device.status?.toLowerCase()
      === 'online',
  ).length

  return (
    <section>
      <div className="flex flex-col justify-between gap-4 md:flex-row md:items-center">
        <div>
          <h1 className="text-3xl font-bold text-white">
            Devices
          </h1>

          <p className="mt-2 text-slate-400">
            Monitor registered cooling units and their health.
          </p>
        </div>

        <button
          type="button"
          onClick={refreshDevices}
          disabled={devicesQuery.isFetching}
          className="flex items-center gap-2 rounded-xl border border-slate-700 bg-slate-900 px-4 py-2.5 text-sm text-slate-300 hover:text-cyan-300 disabled:opacity-50"
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
        <div className="rounded-2xl border border-cyan-500/20 bg-cyan-500/5 p-5">
          <p className="text-sm text-slate-400">
            Total Devices
          </p>

          <p className="mt-2 text-3xl font-bold text-white">
            {devices.length}
          </p>
        </div>

        <div className="rounded-2xl border border-emerald-500/20 bg-emerald-500/5 p-5">
          <p className="text-sm text-slate-400">
            Online
          </p>

          <p className="mt-2 text-3xl font-bold text-emerald-400">
            {onlineCount}
          </p>
        </div>

        <div className="rounded-2xl border border-red-500/20 bg-red-500/5 p-5">
          <p className="text-sm text-slate-400">
            Offline
          </p>

          <p className="mt-2 text-3xl font-bold text-red-400">
            {devices.length - onlineCount}
          </p>
        </div>
      </div>

      {devices.length === 0 ? (
        <div className="mt-8 rounded-2xl border border-dashed border-slate-700 p-12 text-center">
          <Boxes
            className="mx-auto text-slate-600"
            size={40}
          />

          <p className="mt-4 text-slate-400">
            No registered devices found.
          </p>
        </div>
      ) : (
        <div className="mt-8 grid gap-5 xl:grid-cols-2">
          {devices.map((device) => {
            const online =
              device.status?.toLowerCase()
              === 'online'

            return (
              <article
                key={device.device_id}
                className="rounded-2xl border border-slate-800 bg-slate-900/60 p-6 transition hover:border-cyan-500/30"
              >
                <div className="flex items-start justify-between gap-4">
                  <div className="flex items-center gap-4">
                    <div
                      className={[
                        'flex size-12 items-center justify-center rounded-xl',
                        online
                          ? 'bg-emerald-500/15 text-emerald-400'
                          : 'bg-red-500/15 text-red-400',
                      ].join(' ')}
                    >
                      {online
                        ? <Wifi size={24} />
                        : <WifiOff size={24} />}
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
                      'rounded-full px-3 py-1 text-xs font-medium',
                      online
                        ? 'bg-emerald-500/15 text-emerald-400'
                        : 'bg-red-500/15 text-red-400',
                    ].join(' ')}
                  >
                    {online ? 'Online' : 'Offline'}
                  </span>
                </div>

                <div className="mt-6 grid gap-3 sm:grid-cols-3">
                  <div className="rounded-xl bg-slate-950/70 p-4">
                    <Wifi
                      size={18}
                      className={booleanStatus(
                        device.wifi_connected,
                      )}
                    />

                    <p className="mt-3 text-xs text-slate-500">
                      Wi-Fi
                    </p>

                    <p
                      className={[
                        'mt-1 text-sm font-medium',
                        booleanStatus(
                          device.wifi_connected,
                        ),
                      ].join(' ')}
                    >
                      {device.wifi_connected
                        ? 'Connected'
                        : 'Disconnected'}
                    </p>
                  </div>

                  <div className="rounded-xl bg-slate-950/70 p-4">
                    <Cloud
                      size={18}
                      className={booleanStatus(
                        device.mqtt_ready,
                      )}
                    />

                    <p className="mt-3 text-xs text-slate-500">
                      MQTT
                    </p>

                    <p
                      className={[
                        'mt-1 text-sm font-medium',
                        booleanStatus(
                          device.mqtt_ready,
                        ),
                      ].join(' ')}
                    >
                      {device.mqtt_ready
                        ? 'Ready'
                        : 'Not ready'}
                    </p>
                  </div>

                  <div className="rounded-xl bg-slate-950/70 p-4">
                    <Database
                      size={18}
                      className={booleanStatus(
                        device.storage_mounted,
                      )}
                    />

                    <p className="mt-3 text-xs text-slate-500">
                      Storage
                    </p>

                    <p
                      className={[
                        'mt-1 text-sm font-medium',
                        booleanStatus(
                          device.storage_mounted,
                        ),
                      ].join(' ')}
                    >
                      {device.storage_mounted
                        ? 'Mounted'
                        : 'Unavailable'}
                    </p>
                  </div>
                </div>

                <div className="mt-5 grid grid-cols-2 gap-4 border-t border-slate-800 pt-5 text-sm">
                  <div>
                    <p className="text-slate-500">
                      Pending bytes
                    </p>

                    <p className="mt-1 font-medium text-white">
                      {device.pending_bytes ?? 0}
                    </p>
                  </div>

                  <div>
                    <p className="text-slate-500">
                      Cloud errors
                    </p>

                    <p className="mt-1 font-medium text-white">
                      {device.cloud_errors ?? 0}
                    </p>
                  </div>
                </div>

                <Link
                    to={`/devices/${device.device_id}`}
                    className="mt-5 flex items-center justify-center rounded-xl bg-cyan-500/10 px-4 py-2.5 text-sm font-medium text-cyan-300 transition hover:bg-cyan-500/20"
                    >
                    View device details
                    </Link>
              </article>
            )
          })}
        </div>
      )}
    </section>
  )
}


export default DevicesPage