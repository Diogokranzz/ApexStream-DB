'use client'
import { useEffect, useRef } from 'react'

export default function ApexDashboard() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const dataRef = useRef<number[]>(new Array(800).fill(50))
  const statsRef = useRef({ lines: 0, temp: 0 })

  useEffect(() => {
    const ws = new WebSocket('ws://localhost:8080')
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d', { alpha: false })
    if (!ctx) return

    ws.onmessage = (event) => {
      if (typeof event.data === 'string') {
        const payload = JSON.parse(event.data)
        statsRef.current = payload
        dataRef.current.push(payload.temp)
        if (dataRef.current.length > canvas.width) {
          dataRef.current.shift()
        }
      }
    }

    let animationId: number

    const draw = () => {
      ctx.fillStyle = '#0a0a0a'
      ctx.fillRect(0, 0, canvas.width, canvas.height)

      ctx.strokeStyle = '#00ff41'
      ctx.lineWidth = 2
      ctx.beginPath()

      const data = dataRef.current
      const height = canvas.height
      const range = 100

      for (let i = 0; i < data.length; i++) {
        const value = data[i]
        const y = height - ((value / range) * height)
        if (i === 0) ctx.moveTo(i, y)
        else ctx.lineTo(i, y)
      }
      ctx.stroke()

      ctx.fillStyle = '#fff'
      ctx.font = '16px monospace'
      ctx.fillText(`LINES: ${statsRef.current.lines.toLocaleString()}`, 20, 30)
      ctx.fillText(`TEMP:  ${statsRef.current.temp.toFixed(2)}°C`, 20, 50)
      ctx.fillText(`FPS:   60`, 20, 70)

      animationId = requestAnimationFrame(draw)
    }

    draw()

    return () => {
      ws.close()
      cancelAnimationFrame(animationId)
    }
  }, [])

  return (
    <main style={{
      backgroundColor: '#000',
      height: '100vh',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center'
    }}>
      <canvas
        ref={canvasRef}
        width={800}
        height={400}
        style={{ border: '2px solid #333', boxShadow: '0 0 20px rgba(0,255,0,0.2)' }}
      />
    </main>
  )
}
