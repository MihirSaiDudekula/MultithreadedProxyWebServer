const express = require('express');
const app = express();
const port = 3000;
const cors = require('cors');

// Add CORS middleware
app.use(cors());
app.use(express.json());

// Test data store
let users = [];

// GET endpoint
app.get('/test', (req, res, next) => {
    try {
        res.send('Hello from test server!');
    } catch (error) {
        next(error);
    }
});

// GET with query params
app.get('/users', (req, res, next) => {
    try {
        res.json(users);
    } catch (error) {
        next(error);
    }
});

// POST endpoint
app.post('/users', (req, res, next) => {
    try {
        const user = req.body;
        if (!user.id || !user.name || !user.email) {
            throw new Error('Invalid user data. Ensure id, name, and email are provided.');
        }
        users.push(user);
        res.status(201).json(user);
    } catch (error) {
        next(error);
    }
});

// DELETE endpoint
app.delete('/users/:id', (req, res, next) => {
    try {
        const id = req.params.id;
        const initialLength = users.length;
        users = users.filter(user => user.id !== id);
        if (users.length === initialLength) {
            throw new Error(`User with id ${id} not found.`);
        }
        res.status(200).send(`User ${id} deleted`);
    } catch (error) {
        next(error);
    }
});

// Large response endpoint
app.get('/large', (req, res, next) => {
    try {
        // Increase to 10MB and add some variation to make it more realistic
        const baseData = 'X'.repeat(1024 * 1024); // 1MB chunk
        let largeData = '';
        for (let i = 0; i < 10; i++) {
            // Add some variation to each MB chunk
            largeData += baseData.replace(/X/g, String.fromCharCode(65 + i));
        }
        res.send(largeData);
    } catch (error) {
        next(error);
    }
});

// Global error-handling middleware
app.use((err, req, res, next) => {
    console.error(err.stack);
    res.status(500).json({
        error: 'Internal Server Error',
        message: err.message,
    });
});

// Listen on all interfaces
app.listen(port, '0.0.0.0', () => {
    console.log(`Test server running at http://0.0.0.0:${port}`);
});
